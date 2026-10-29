#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gui.hpp"
#include "ui_state.hpp"
#include "theme_palette.hpp"
#include "surface_style.hpp"
#include "../ui/grim_ui.hpp"
#include "../ui/hud_editor.hpp"
#include "../ui/config_page.hpp"
#include "../configs/keybind_profile.hpp"
#include "../ui/themes_page.hpp"
#include "../ui/menu_background.hpp"
#include <Windows.h>
#include <gl/GL.h>
#include <wincodec.h>
#include <commdlg.h>
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_win32.h"
#include "../modules/core/module_registry.hpp"
#include "../modules/combat/shield_breaker.hpp"
#include "../modules/combat/auto_totem.hpp"
#include "../modules/combat/jump_reset.hpp"
#include "../modules/combat/triggerbot.hpp"
#include "../friends/friends.hpp"
#include "../loader/loader_ui.hpp"
#include "../platform/input/key_codes.hpp"
#include "../services/input/input_service.hpp"
#include "../services/game/menu_input.hpp"
#include "../services/game/menu_input_policy.hpp"
#include "../../resource.h"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cfloat>
#include <array>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "comdlg32.lib")

namespace
{
	typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);

	wglSwapBuffers_t wglSwapBuffers = nullptr;
	wglSwapBuffers_t original_wglSwapBuffers = nullptr;
	WNDPROC original_WndProc = nullptr;

	HWND window = nullptr;
	ImGuiContext* imGuiContext = nullptr;
	HGLRC original_context = nullptr;
	HGLRC new_context = nullptr;

	std::atomic_bool request_shutdown{ false };
	std::atomic_bool shut_down{ false };
	std::atomic_bool focus_reset_pending{ false };
	volatile bool run_once = true;

	int last_pressed_key = 0;
	ImVector<ImWchar> text_glyph_ranges{};
	ImVector<ImWchar> icon_glyph_ranges{};
	std::vector<unsigned char> ui_font_data{};
	std::vector<unsigned char> ui_bold_font_data{};
	std::vector<unsigned char> icon_font_data{};
	std::array<GLuint, static_cast<size_t>(gui::icon_asset::count)> icon_textures{};
	std::array<bool, static_cast<size_t>(gui::icon_asset::count)> icon_texture_attempted{};
	struct background_texture { GLuint id = 0; ImVec2 size{}; bool attempted = false; };
	std::array<background_texture, static_cast<std::size_t>(gui::background::style::count)> backgrounds{};
	std::mutex background_import_mutex;
	gui::background::import_state background_import_status = gui::background::import_state::idle;
	std::vector<unsigned char> pending_background;
	unsigned background_generation = 0;
	struct picker_job { HMODULE module; HWND owner; unsigned generation; };

	std::filesystem::path custom_background_path()
	{
		wchar_t directory[32768]{};
		const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, static_cast<DWORD>(std::size(directory)));
		if (!length || length >= std::size(directory)) return {};
		return std::filesystem::path(directory) / L"onestep" / L"menu-background.image";
	}

	bool read_background_file(const std::filesystem::path& path, std::vector<unsigned char>& bytes)
	{
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input) return false;
		const auto size = input.tellg();
		if (size <= 0 || size > static_cast<std::streamoff>(gui::background::max_file_bytes)) return false;
		bytes.resize(static_cast<std::size_t>(size));
		input.seekg(0);
		return static_cast<bool>(input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())));
	}

	DWORD WINAPI choose_background_file(void* parameter)
	{
		const HMODULE reference = static_cast<picker_job*>(parameter)->module;
		{
			std::unique_ptr<picker_job> job(static_cast<picker_job*>(parameter));
			try
			{
				wchar_t path[32768]{};
				OPENFILENAMEW dialog{};
				dialog.lStructSize = sizeof(dialog);
				dialog.hwndOwner = job->owner;
				dialog.lpstrFilter = L"PNG / JPEG\0*.png;*.jpg;*.jpeg\0\0";
				dialog.lpstrFile = path;
				dialog.nMaxFile = static_cast<DWORD>(std::size(path));
				dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
					OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_DONTADDTORECENT;
				const bool chosen = GetOpenFileNameW(&dialog) != FALSE;
				const bool dialog_failed = !chosen && CommDlgExtendedError() != 0;
				std::vector<unsigned char> bytes;
				const bool valid = chosen && read_background_file(path, bytes);
				std::lock_guard lock(background_import_mutex);
				if (job->generation == background_generation)
				{
					pending_background = valid ? std::move(bytes) : std::vector<unsigned char>{};
					background_import_status = (chosen && !valid) || dialog_failed
						? gui::background::import_state::failed : gui::background::import_state::idle;
				}
			}
			catch (...)
			{
				std::lock_guard lock(background_import_mutex);
				if (job->generation == background_generation)
					background_import_status = gui::background::import_state::failed;
			}
		}
		// A picker can outlive menu unload. Keep the DLL loaded until no picker
		// code or C++ objects are executing, then release it atomically with exit.
		FreeLibraryAndExitThread(reference, 0);
	}

	int icon_resource(gui::icon_asset asset)
	{
		switch (asset)
		{
		case gui::icon_asset::helmet: return IDR_ICON_HELMET;
		case gui::icon_asset::background: return IDR_BACKGROUND_MAIN;
		case gui::icon_asset::spotify: return IDR_ICON_SPOTIFY;
        case gui::icon_asset::wing_dragon: return IDR_WING_DRAGON;
        case gui::icon_asset::wing_angel: return IDR_WING_ANGEL;
        case gui::icon_asset::wing_butterfly: return IDR_WING_BUTTERFLY;
        case gui::icon_asset::cape_phoenix: return IDR_CAPE_PHOENIX;
        case gui::icon_asset::cape_astral: return IDR_CAPE_ASTRAL;
		default: return 0;
		}
	}

	HMODULE current_module()
	{
		HMODULE module = nullptr;
		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&gui::get_icon_texture),
			&module);
		return module;
	}

	bool read_resource_bytes(int resource_id, std::vector<unsigned char>& output)
	{
		output.clear();
		const HMODULE module = current_module();
		const HRSRC resource = module && resource_id
			? FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA)
			: nullptr;
		if (!resource) return false;
		const DWORD size = SizeofResource(module, resource);
		const HGLOBAL loaded = LoadResource(module, resource);
		const void* data = loaded ? LockResource(loaded) : nullptr;
		if (!data || size == 0 || size > 16u * 1024u * 1024u) return false;
		const auto* begin = static_cast<const unsigned char*>(data);
		output.assign(begin, begin + size);
		return true;
	}

	bool load_image_texture(const void* encoded_data, std::size_t encoded_size, GLuint& texture, ImVec2* dimensions = nullptr)
	{
		IWICImagingFactory* factory = nullptr;
		IWICStream* stream = nullptr;
		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		UINT width = 0;
		UINT height = 0;
		GLint maximum_texture_size = 0;
		GLint uploaded_width = 0;
		GUID container{};
		std::vector<BYTE> pixels;
		bool success = false;
		const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		if (!encoded_data || encoded_size == 0 || encoded_size > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()))
			goto cleanup;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&factory))) || !factory)
			goto cleanup;
		if (FAILED(factory->CreateStream(&stream)) || !stream ||
			FAILED(stream->InitializeFromMemory(
				const_cast<BYTE*>(static_cast<const BYTE*>(encoded_data)), static_cast<DWORD>(encoded_size))))
			goto cleanup;
		if (FAILED(factory->CreateDecoderFromStream(stream, nullptr,
			WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder)
			goto cleanup;
		if (FAILED(decoder->GetContainerFormat(&container)) ||
			(!IsEqualGUID(container, GUID_ContainerFormatPng) && !IsEqualGUID(container, GUID_ContainerFormatJpeg)))
			goto cleanup;
		if (FAILED(decoder->GetFrame(0, &frame)) || !frame ||
			FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
			FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
				nullptr, 0.0, WICBitmapPaletteTypeCustom)))
			goto cleanup;

		if (FAILED(converter->GetSize(&width, &height)) || !gui::background::valid_dimensions(width, height))
			goto cleanup;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture_size);
		if (width > static_cast<UINT>(maximum_texture_size) || height > static_cast<UINT>(maximum_texture_size))
			goto cleanup;

		pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
		if (FAILED(converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data())))
			goto cleanup;

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

		glGenTextures(1, &texture);
		if (!texture) goto cleanup;
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
			0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploaded_width);
		if (!texture || uploaded_width != static_cast<GLint>(width))
		{
			if (texture) glDeleteTextures(1, &texture);
			texture = 0;
			glBindTexture(GL_TEXTURE_2D, 0);
			goto cleanup;
		}

		typedef void (APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLenum target);
		static PFNGLGENERATEMIPMAPPROC pfn_glGenerateMipmap = nullptr;
		static bool pfn_resolved = false;
		if (!pfn_resolved)
		{
			pfn_glGenerateMipmap = reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(wglGetProcAddress("glGenerateMipmap"));
			pfn_resolved = true;
		}

		if (pfn_glGenerateMipmap)
		{
			pfn_glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		success = texture != 0;
		if (success && dimensions) *dimensions = ImVec2(static_cast<float>(width), static_cast<float>(height));

	cleanup:
		if (converter) converter->Release();
		if (frame) frame->Release();
		if (decoder) decoder->Release();
		if (stream) stream->Release();
		if (factory) factory->Release();
		if (SUCCEEDED(com)) CoUninitialize();
		return success;
	}

	bool panel_was_open = false;

	void sync_panel_input()
	{
		const bool open = gui::draw;
		const bool focused = window && GetForegroundWindow() == window;
		const bool opening = open && !panel_was_open;
		if (open != panel_was_open)
		{
			services::input::reset_physical_buttons();
			panel_was_open = open;
		}
		services::game::menu_input::update(open, focused, !gui::should_unload);
		// Software cursor appears on this frame even without WM_MOUSEMOVE or
		// WM_SETCURSOR. The Win32 backend hides the OS cursor to avoid doubling.
		ImGui::GetIO().MouseDrawCursor = open && focused;
		if (!open || !focused) return;
		ClipCursor(nullptr);
		SetCursor(nullptr);
		if (opening)
		{
			RECT client{};
			if (GetClientRect(window, &client))
			{
				POINT point{client.right / 2, client.bottom / 2};
				if (ClientToScreen(window, &point)) SetCursorPos(point.x, point.y);
			}
		}
	}

	void focus_minecraft_window(HWND minecraft_window)
	{
		if (!minecraft_window || !IsWindow(minecraft_window))
			return;

		if (IsIconic(minecraft_window))
			ShowWindow(minecraft_window, SW_RESTORE);

		const HWND foreground_window = GetForegroundWindow();
		const DWORD current_thread = GetCurrentThreadId();
		const DWORD foreground_thread = foreground_window
			? GetWindowThreadProcessId(foreground_window, nullptr)
			: 0;
		const bool attached = foreground_thread != 0 && foreground_thread != current_thread &&
			AttachThreadInput(current_thread, foreground_thread, TRUE) != FALSE;

		BringWindowToTop(minecraft_window);
		SetForegroundWindow(minecraft_window);
		SetActiveWindow(minecraft_window);
		SetFocus(minecraft_window);

		if (attached)
			AttachThreadInput(current_thread, foreground_thread, FALSE);
	}



} // Closing anonymous namespace

void gui::render_theme_selection()
{
	gui::themes_page::render();
}

void gui::request_panic()
{
	for (modules::module* module : modules::get_modules())
		module->enabled = false;
	gui::draw = false;
	gui::should_unload = true;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void release_panel_mouse(HWND hWnd)
{
	if (ImGui::GetCurrentContext())
	{
		// Reset the backend's button mask as well as ImGui's state. These are
		// internal UI messages, never SendInput events or game button releases.
		for (const UINT message : {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP})
			ImGui_ImplWin32_WndProcHandler(hWnd, message, 0, 0);
		ImGui_ImplWin32_WndProcHandler(hWnd, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON1), 0);
		ImGui_ImplWin32_WndProcHandler(hWnd, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON2), 0);
		auto& io = ImGui::GetIO();
		io.AddFocusEvent(false);
		io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
		io.MouseDrawCursor = false;
	}
	if (GetCapture() == hWnd) ReleaseCapture();
	ClipCursor(nullptr);
}
static LRESULT CALLBACK detour_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HCURSOR arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);
	static bool physical_left_down = false;
	static bool physical_right_down = false;
	static bool prev_gui_draw = false;

	if (prev_gui_draw != gui::draw)
	{
		prev_gui_draw = gui::draw;
		physical_left_down = physical_right_down = false;
	}

	auto update_physical_button = [&](bool left, bool pressed)
	{
		bool& previous = left ? physical_left_down : physical_right_down;
		if (previous == pressed)
			return;
		previous = pressed;
		services::input::observe_physical(left ? services::input::mouse_button::left : services::input::mouse_button::right, pressed);
		if (left)
		{
			if (pressed)
			{
				// Shield Breaker has priority when the crosshair is on a blocking
				// player. The slot swap happens before this input reaches GLFW, so
				// Minecraft processes the original physical click with the axe.
				modules::shield_breaker::gate_left_click();
			}
		}
	};

	// GLFW commonly receives hardware mouse buttons through raw input. Reading
	// WM_INPUT here keeps manual clicks separate from our tagged SendInput pulses.
	const bool focus_lost = msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && !wParam) ||
		(msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE);
	if (focus_lost)
	{
		focus_reset_pending.store(true, std::memory_order_release);
		physical_left_down = physical_right_down = false;
		services::input::reset_physical_buttons();
        if (auto* totem = dynamic_cast<modules::auto_totem*>(modules::find_module_by_id("auto_totem")))
            totem->cancel_on_client_thread();
		if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
			jump->cancel_on_client_thread();
		if (gui::draw) release_panel_mouse(hWnd);
	}
	if (msg == WM_SETFOCUS)
		services::input::reset_physical_keys();
	if (focus_lost || msg == WM_SETFOCUS)
	{
		if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
	}
	if (msg == WM_INPUT)
	{
		RAWINPUT raw{};
		UINT raw_size = sizeof(raw);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
			&raw, &raw_size, sizeof(RAWINPUTHEADER)) == sizeof(raw) &&
			raw.header.dwType == RIM_TYPEMOUSE)
		{
			const USHORT flags = raw.data.mouse.usButtonFlags;
			const auto tag = static_cast<std::uintptr_t>(raw.data.mouse.ulExtraInformation);
			if (!gui::draw && !services::input::is_synthetic_tag(tag))
			{
				if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) update_physical_button(true, true);
				if (flags & RI_MOUSE_LEFT_BUTTON_UP) update_physical_button(true, false);
				if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) update_physical_button(false, true);
				if (flags & RI_MOUSE_RIGHT_BUTTON_UP) update_physical_button(false, false);
			}
		}
	}
	const auto mouse_tag = static_cast<std::uintptr_t>(GetMessageExtraInfo());
	const bool key_down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
	const bool key_up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
	if (gui::draw && (key_down || key_up) && services::game::menu_input::system_shortcut(
		static_cast<unsigned>(wParam), msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || (lParam & (1LL << 29))))
		return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
	if ((key_down || key_up) && !services::input::is_synthetic_tag(mouse_tag))
		services::input::observe_physical_key(static_cast<unsigned>(wParam), static_cast<unsigned>((lParam >> 16) & 0x1ff), key_down);
	if (!services::input::is_synthetic_tag(mouse_tag))
	{
		if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP)
			services::input::observe_physical_key(VK_MBUTTON, 0, msg == WM_MBUTTONDOWN && !gui::draw);
		if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP)
			services::input::observe_physical_key(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2,
				0, msg == WM_XBUTTONDOWN && !gui::draw);
	}

	if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !services::input::is_synthetic_tag(mouse_tag))
		last_pressed_key = platform::keys::from_message(static_cast<int>(wParam), static_cast<unsigned>((lParam >> 16) & 0x1ff));
	else if (msg == WM_MBUTTONDOWN)
		last_pressed_key = VK_MBUTTON;
	else if (msg == WM_XBUTTONDOWN)
		last_pressed_key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;

	// Consume the complete closing Escape gesture, including autorepeat. It
	// must not also open Minecraft's pause menu after the ClickGUI disappears.
	if ((key_down || key_up) && wParam == VK_ESCAPE &&
		gui::suppress_escape_until_release.load(std::memory_order_acquire))
	{
		if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		if (key_up) gui::suppress_escape_until_release.store(false, std::memory_order_release);
		return 0;
	}

	if (gui::draw)
	{
		// Consume raw input without forwarding it to GLFW, but still perform
		// Windows' required raw-input cleanup after the tracking above.
		if (msg == WM_INPUT) return DefWindowProcA(hWnd, msg, wParam, lParam);
		const bool focused = GetForegroundWindow() == hWnd;
		if (msg == WM_SETCURSOR && focused && LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(ImGui::GetIO().MouseDrawCursor ? nullptr : arrow_cursor);
			return TRUE;
		}

		// Do not swallow OS window switching/closing shortcuts. Ordinary game
		// keys and mouse clicks still stay inside the panel.
		if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		const bool keyboard_message = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || msg == WM_IME_CHAR;
		const bool mouse_message = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSELEAVE;
		// Forward releases exactly once so GLFW cannot retain a pre-menu hold.
		if (key_up) return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
		if (keyboard_message || mouse_message) return 0;
		// Focus, activation and resize still belong to GLFW/the game window.
		return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
	}

	// Legacy mouse messages remain as a fallback. The transition guard above
	// prevents a raw-input + legacy-input pair from firing the modules twice.
	if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) &&
		!services::input::is_synthetic_tag(mouse_tag))
	{
		update_physical_button(true, msg == WM_LBUTTONDOWN);
	}
	if ((msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) &&
		!services::input::is_synthetic_tag(mouse_tag))
	{
		update_physical_button(false, msg == WM_RBUTTONDOWN);
	}

	return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
}

static void uninit(HWND hWnd, HDC device)
{
    services::game::menu_input::shutdown();
	panel_was_open = false;
    for (auto* module : modules::get_modules())
        module->on_render_shutdown();
	// Release owned jump input while client-thread JNI and the input guard are
	// still available, before module destruction/unhooking on the worker.
	if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
		jump->cancel_on_client_thread();
	// UI textures are owned by the injected OpenGL context. Release them while
	// that context is still valid so repeated window recreation and unloads do
	// not leak the embedded logo/background textures.
	if (new_context)
	{
		wglMakeCurrent(device, new_context);
		gui::background::shutdown();
		for (GLuint& texture : icon_textures)
		{
			if (texture)
			{
				glDeleteTextures(1, &texture);
				texture = 0;
			}
		}
		icon_texture_attempted.fill(false);
	}
	gui::grim::reset_runtime_state();
	configs::keybind_profile::flush(true);
	gui::reset_widget_runtime_state();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(imGuiContext);
	SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)original_WndProc);
	wglMakeCurrent(device, original_context);
	wglDeleteContext(new_context);
}

static BOOL WINAPI detour_wglSwapBuffers(HDC device)
{
	HWND current_window = WindowFromDC(device);


	if (request_shutdown.load(std::memory_order_acquire))
	{
		request_shutdown.store(false, std::memory_order_release);
		MH_DisableHook(wglSwapBuffers);
		uninit(current_window, device);

		// Finish the current swap through MinHook's trampoline before telling
		// the worker thread that the render side is safe to remove. Signalling
		// before this call allowed the DLL to unload while this detour was active.
		const BOOL result = original_wglSwapBuffers
			? original_wglSwapBuffers(device)
			: TRUE;
		shut_down.store(true, std::memory_order_release);
		return result;
	}

	HGLRC last_context = wglGetCurrentContext();

	// if already init, and window changed, cleanup and reinit
	if (!run_once && window != current_window)
	{
		uninit(current_window, device);
		run_once = true;
	}

	if (run_once)
	{
		window = current_window;
		if (!window) {
			loader_ui::fail("Minecraft render window was not available");
		}
		else
		{
			// Injection happens from another process, so explicitly return the
			// user to Minecraft before the first loader frame is presented.
			focus_minecraft_window(window);
		}

		original_context = last_context;
		new_context = wglCreateContext(device);
		if (last_context && new_context)
		{
			wglShareLists(last_context, new_context);
		}
		wglMakeCurrent(device, new_context);

		original_WndProc = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)detour_WndProc);
		if (!original_WndProc) {
			loader_ui::fail("Minecraft input window hook failed");
		}

		imGuiContext = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		// Do NOT set NavEnableKeyboard – it intercepts every key including INSERT
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		ImFontGlyphRangesBuilder text_range_builder;
		text_range_builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
		// Keep extended Latin glyphs for player names and user-authored text.
		static constexpr ImWchar user_text_ranges[]{0x0100, 0x017f, 0};
		text_range_builder.AddRanges(user_text_ranges);
		text_range_builder.BuildRanges(&text_glyph_ranges);
		ImFontConfig ui_font_config{};
		ui_font_config.OversampleH = 3;
		ui_font_config.OversampleV = 1;
		ui_font_config.PixelSnapH = false;
		ui_font_config.RasterizerMultiply = 1.0f;
		ui_font_config.FontDataOwnedByAtlas = false;
		if (read_resource_bytes(IDR_FONT_MONTSERRAT_SEMIBOLD, ui_font_data))
			gui::ui_font = io.Fonts->AddFontFromMemoryTTF(
				ui_font_data.data(), static_cast<int>(ui_font_data.size()), 19.5f,
				&ui_font_config, text_glyph_ranges.Data);
		if (!gui::ui_font) {
			gui::ui_font = io.Fonts->AddFontDefault();
		}
		ImFontConfig bold_font_config = ui_font_config;
		bold_font_config.RasterizerMultiply = 1.0f;
		if (read_resource_bytes(IDR_FONT_MONTSERRAT_BOLD, ui_bold_font_data))
			gui::ui_bold_font = io.Fonts->AddFontFromMemoryTTF(
				ui_bold_font_data.data(), static_cast<int>(ui_bold_font_data.size()), 22.5f,
				&bold_font_config, text_glyph_ranges.Data);
		if (!gui::ui_bold_font)
			gui::ui_bold_font = gui::ui_font;
		io.FontDefault = gui::ui_font;

		// High-resolution font for crisp, non-pixelated watermark rendering
		ImFontConfig wm_font_config = ui_font_config;
		wm_font_config.OversampleH = 2;
		wm_font_config.OversampleV = 1;
		wm_font_config.PixelSnapH = false;
		wm_font_config.RasterizerMultiply = 1.0f;
		if (!ui_bold_font_data.empty())
			gui::watermark_font = io.Fonts->AddFontFromMemoryTTF(
				ui_bold_font_data.data(), static_cast<int>(ui_bold_font_data.size()), 56.0f,
				&wm_font_config, text_glyph_ranges.Data);
		if (!gui::watermark_font)
			gui::watermark_font = gui::ui_bold_font;

		ImFontConfig icon_font_config{};
		icon_font_config.OversampleH = 3;
		icon_font_config.OversampleV = 1;
		icon_font_config.PixelSnapH = false;
		icon_font_config.RasterizerMultiply = 1.0f;
		icon_font_config.FontDataOwnedByAtlas = false;
		ImFontGlyphRangesBuilder icon_range_builder;
		icon_range_builder.AddChar(0xF05B);
		icon_range_builder.AddChar(0xF121);
		icon_range_builder.AddChar(0xE163);
		icon_range_builder.AddChar(0xF06E);
		icon_range_builder.AddChar(0xF0C0);
		icon_range_builder.AddChar(0xF07C);
		icon_range_builder.AddChar(0xF1DE);
		icon_range_builder.AddChar(0xF019);
		icon_range_builder.AddChar(0xF0C7);
		icon_range_builder.AddChar(0xF1F8);
		icon_range_builder.AddChar(0xF00D);
		icon_range_builder.AddChar(0xF021);
		icon_range_builder.AddChar(0xF234);
		icon_range_builder.AddChar(0xF503);
		icon_range_builder.AddChar(0xF554);
		icon_range_builder.AddChar(0xF0E7); // bolt: Vape
		icon_range_builder.AddChar(0xF53F); // palette: Themes
		icon_range_builder.AddChar(0xF007);
		icon_range_builder.AddChar(0xF013);
		icon_range_builder.AddChar(0xF07B);
		icon_range_builder.AddChar(0xF108);
		icon_range_builder.AddChar(0xF11C); // keyboard: Keybinds
		icon_range_builder.AddChar(0xF005);
		icon_range_builder.AddChar(0x002B);
		icon_range_builder.BuildRanges(&icon_glyph_ranges);
		if (read_resource_bytes(IDR_FONT_FONTAWESOME_SOLID, icon_font_data))
			gui::icon_font = io.Fonts->AddFontFromMemoryTTF(
				icon_font_data.data(), static_cast<int>(icon_font_data.size()), 20.0f,
				&icon_font_config, icon_glyph_ranges.Data);

		ImFontConfig arraylist_font_config = ui_font_config;
		gui::arraylist_font = ui_font_data.empty() ? nullptr : io.Fonts->AddFontFromMemoryTTF(
			ui_font_data.data(), static_cast<int>(ui_font_data.size()), 16.5f,
			&arraylist_font_config, text_glyph_ranges.Data);
		if (!gui::arraylist_font) {
			gui::arraylist_font = gui::ui_font;
		}
		
		// Timewarp-inspired dark card theme.
		{
			auto& style = ImGui::GetStyle();
			ImVec4* colors = style.Colors;

			// Anti-aliased geometry with smooth curve tessellation
			gui::surface_style::configure(style);

			style.WindowRounding = 18.0f;
			style.FrameRounding = 8.0f;
			style.GrabRounding = 8.0f;
			style.PopupRounding = 10.0f;
			style.ChildRounding = 13.0f;
			style.ScrollbarRounding = 8.0f;
			style.ScrollbarSize = 0.0f;
			style.FramePadding = ImVec2(10, 7);
			style.WindowPadding = ImVec2(14, 14);
			style.ItemSpacing = ImVec2(10, 9);
			style.ItemInnerSpacing = ImVec2(8, 8);
			style.WindowBorderSize = 1.0f;
			style.ChildBorderSize = 1.0f;
			style.PopupBorderSize = 1.0f;
			style.FrameBorderSize = 0.0f;

			// Deep charcoal palette from the reference layout.
			colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
			colors[ImGuiCol_TextDisabled]           = ImVec4(0.43f, 0.43f, 0.47f, 1.00f);
			colors[ImGuiCol_WindowBg]               = ImVec4(0.015f, 0.015f, 0.021f, 0.74f);
			colors[ImGuiCol_ChildBg]                = ImVec4(0.024f, 0.024f, 0.032f, 0.60f);
			colors[ImGuiCol_PopupBg]                = ImVec4(0.020f, 0.018f, 0.027f, 0.92f);
			colors[ImGuiCol_Border]                 = ImVec4(0.62f, 0.32f, 1.00f, 0.12f);
			colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			
			colors[ImGuiCol_FrameBg]                = ImVec4(0.055f, 0.052f, 0.068f, 0.68f);
			colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.090f, 0.082f, 0.110f, 0.80f);
			colors[ImGuiCol_FrameBgActive]          = ImVec4(0.120f, 0.105f, 0.150f, 0.88f);
			
			colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			
			colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.08f, 0.08f, 0.09f, 0.00f);
			colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			
			// Rose accent used by the selected navigation pill.
			colors[ImGuiCol_CheckMark]              = ImVec4(0.62f, 0.30f, 1.00f, 1.00f);
			colors[ImGuiCol_SliderGrab]             = ImVec4(0.62f, 0.30f, 1.00f, 1.00f);
			colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.76f, 0.47f, 1.00f, 1.00f);
			
			colors[ImGuiCol_Button]                 = ImVec4(0.085f, 0.050f, 0.120f, 0.72f);
			colors[ImGuiCol_ButtonHovered]          = ImVec4(0.66f, 0.36f, 0.96f, 0.85f);
			colors[ImGuiCol_ButtonActive]           = ImVec4(0.66f, 0.36f, 0.96f, 1.00f);
			
			colors[ImGuiCol_Header]                 = ImVec4(0.16f, 0.09f, 0.23f, 0.72f);
			colors[ImGuiCol_HeaderHovered]          = ImVec4(0.66f, 0.36f, 0.96f, 0.40f);
			colors[ImGuiCol_HeaderActive]           = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			
			colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.17f, 0.22f, 1.00f);
			colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			colors[ImGuiCol_SeparatorActive]        = ImVec4(0.66f, 0.36f, 0.96f, 1.00f);
			
			colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
			colors[ImGuiCol_TabHovered]             = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			colors[ImGuiCol_TabActive]              = ImVec4(0.16f, 0.15f, 0.20f, 1.00f);
			colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.16f, 0.15f, 0.20f, 1.00f);
		}

		ImGui_ImplOpenGL3_Init();
		ImGui_ImplWin32_Init(window);
		run_once = false;
	}

	wglMakeCurrent(device, new_context);

	const bool focus_interrupted = focus_reset_pending.exchange(false, std::memory_order_acq_rel);

	// Poll the fixed Right Shift shortcut every frame. GLFW does not reliably
	// expose every Windows virtual key through the hooked WndProc path.
	{
		static services::game::menu_input::focused_shortcut menu_shortcut;
		if (focus_interrupted) menu_shortcut.pressed(false, false);
		const bool focused = window && GetForegroundWindow() == window;
		const bool current_menu_key = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
		const bool shortcut_pressed = menu_shortcut.pressed(focused, current_menu_key);
		if (gui::suppress_menu_key_until_release)
		{
			if (!current_menu_key)
				gui::suppress_menu_key_until_release = false;
		}
		else if (shortcut_pressed &&
			!std::any_of(modules::get_modules().begin(), modules::get_modules().end(),
				[](const modules::module* entry) { return entry->is_keybind_capture_active(); }) &&
			!gui::should_unload && !gui::loader_visible)
		{
			gui::draw = !gui::draw;
		}
	}
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0)
		gui::suppress_escape_until_release.store(false, std::memory_order_release);

	sync_panel_input();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	static services::game::menu_input::focused_pointer panel_pointer;
	if (focus_interrupted) panel_pointer.accepts(false, false);
	const bool pointer_focused = services::game::menu_input::accepts_panel_pointer(gui::draw, window && GetForegroundWindow() == window);
	const bool left_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	const bool right_down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	const bool accept_buttons = panel_pointer.accepts(pointer_focused, left_down || right_down);
	if (pointer_focused)
	{
		ClipCursor(NULL);
		POINT mouse_pos{};
		if (GetCursorPos(&mouse_pos) && ScreenToClient(window, &mouse_pos))
		{
			ImGui::GetIO().AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
		}
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, accept_buttons && left_down);
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, accept_buttons && right_down);
	}
	ImGui::NewFrame();


	// Update dynamic accent colors in style
	{
		auto& style = ImGui::GetStyle();
		ImColor accent = gui::get_dynamic_color();
		const ImVec4 base = accent.Value;
		ImColor accent_hover(base.x + (1.0f - base.x) * 0.14f, base.y + (1.0f - base.y) * 0.14f,
			base.z + (1.0f - base.z) * 0.14f, 1.0f);
		ImColor accent_active(base.x * 0.84f, base.y * 0.84f, base.z * 0.84f, 1.0f);

		style.Colors[ImGuiCol_CheckMark]        = accent;
		style.Colors[ImGuiCol_SliderGrab]       = accent;
		style.Colors[ImGuiCol_SliderGrabActive] = accent_active;
		style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.85f);
		style.Colors[ImGuiCol_ButtonActive]     = accent_active;
		style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.40f);
		style.Colors[ImGuiCol_HeaderActive]     = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.80f);
		style.Colors[ImGuiCol_TabHovered]       = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.80f);
		style.Colors[ImGuiCol_Header]           = ImVec4(base.x, base.y, base.z, 0.20f);
		style.Colors[ImGuiCol_TextSelectedBg]   = ImVec4(base.x, base.y, base.z, 0.30f);
		style.Colors[ImGuiCol_NavHighlight]     = accent;
		style.Colors[ImGuiCol_Border]           = ImVec4(base.x, base.y, base.z, 0.12f);
		style.Colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.05f + base.x * 0.07f, 0.05f + base.y * 0.07f, 0.06f + base.z * 0.07f, 0.80f);
		style.Colors[ImGuiCol_FrameBgActive]    = ImVec4(0.05f + base.x * 0.12f, 0.05f + base.y * 0.12f, 0.06f + base.z * 0.12f, 0.88f);
		style.Colors[ImGuiCol_Button]           = ImVec4(0.04f + base.x * 0.06f, 0.04f + base.y * 0.06f, 0.05f + base.z * 0.06f, 0.72f);
		style.Colors[ImGuiCol_SeparatorHovered] = accent_hover;
		style.Colors[ImGuiCol_SeparatorActive]  = accent_active;
		style.Colors[ImGuiCol_ScrollbarGrab]    = ImVec4(0.72f, 0.74f, 0.80f, 0.18f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(base.x, base.y, base.z, 0.55f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = accent;
	}

	// Runtime initialization controls when gameplay rendering starts.
	// Inside Minecraft we only keep a headless readiness gate so gameplay
	// modules cannot run before JNI/JVMTI initialization has finished.
	if (gui::loader_visible)
	{
		const loader_ui::state loader_state = loader_ui::snapshot();
		if (loader_state.completed || loader_state.closing)
			gui::loader_visible = false;
		else if (loader_state.failed)
		{
			gui::loader_visible = false;
			gui::should_unload = true;
		}
	}

	if (!gui::draw)
	{
		if (gui::hud_layout_editor_active) gui::hud_editor::close();
		gui::hud_editor_active = false;
		gui::hud_layout_editor_active = false;
	}

	// Render HUD and ArrayList overlays
	gui::hud_editor::restore_saved_layout();
	gui::hud_editor_active = gui::draw && gui::hud_layout_editor_active;
	if (!gui::loader_visible)
	{
		for (modules::module* module : modules::get_modules())
		{
			if (module->enabled || module->render_when_disabled())
				module->render_overlay();
		}
	}
	else
	{
        if (auto* totem = dynamic_cast<modules::auto_totem*>(modules::find_module_by_id("auto_totem")))
            totem->cancel_on_client_thread();
		if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
			jump->cancel_on_client_thread();
        if (auto* trigger = dynamic_cast<modules::triggerbot*>(modules::find_module_by_id("triggerbot")))
            trigger->cancel_on_client_thread();
	}

	if (gui::draw && !gui::loader_visible)
	{
		const ImGuiViewport* reference_viewport = ImGui::GetMainViewport();

		gui::grim::page_callbacks callbacks{};
		callbacks.friends = []() { friends::render_manager(true); };
		callbacks.configs = []() { gui::config_page::render(true); };
		gui::grim::render(reference_viewport, callbacks);


	}
	// Escape can close the panel inside the UI render. Restore capture on
	// that same frame and neutralize any input written by earlier callbacks.
	sync_panel_input();
	gui::ui_state::flush_preferences(false, ImGui::IsAnyItemActive());
	configs::keybind_profile::flush();
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (last_context)
	{
		wglMakeCurrent(device, last_context);
	}
	else if (original_context)
	{
		wglMakeCurrent(device, original_context);
	}
	return original_wglSwapBuffers(device);
}

bool gui::init()
{
	gui::ui_state::load_preferences();
	HMODULE opengl = GetModuleHandleA("opengl32.dll");
	if (!opengl) 
	{
		loader_ui::fail("OpenGL runtime was not loaded");
		return false;
	}
	wglSwapBuffers = (wglSwapBuffers_t)GetProcAddress(opengl, "wglSwapBuffers");

	if (!wglSwapBuffers)
	{
		loader_ui::fail("wglSwapBuffers export was not found");
		return false;
	}

	if (MH_Initialize() != MH_OK) 
	{
		loader_ui::fail("Native hook engine initialization failed");
		return false;
	}

	if (MH_CreateHook(wglSwapBuffers, detour_wglSwapBuffers, (void**)&original_wglSwapBuffers) != MH_OK) 
	{
		loader_ui::fail("OpenGL hook creation failed");
		return false;
	}

	if (MH_EnableHook(wglSwapBuffers) != MH_OK) 
	{
		loader_ui::fail("OpenGL hook activation failed");
		return false;
	}
	return true;
}

ImTextureID gui::get_icon_texture(icon_asset asset)
{
	const size_t index = static_cast<size_t>(asset);
	if (index >= icon_textures.size())
		return nullptr;

	if (!icon_texture_attempted[index])
	{
		icon_texture_attempted[index] = true;
		std::vector<unsigned char> encoded_icon;
		if (read_resource_bytes(icon_resource(asset), encoded_icon))
			load_image_texture(encoded_icon.data(), encoded_icon.size(), icon_textures[index]);
	}

	return reinterpret_cast<ImTextureID>(static_cast<intptr_t>(icon_textures[index]));
}

gui::background::texture_view gui::background::texture(style selected)
{
	if (selected <= style::classic || selected >= style::count) return {};
	auto& image = backgrounds[static_cast<std::size_t>(selected)];
	if (!image.attempted)
	{
		image.attempted = true;
		std::vector<unsigned char> bytes;
		const int resources[] = { 0, IDR_BACKGROUND_MAIN, IDR_SKY_CANDY_DUSK, IDR_SKY_NEBULA, IDR_SKY_AURORA, 0 };
		if (selected == style::custom ? read_background_file(custom_background_path(), bytes) :
			read_resource_bytes(resources[static_cast<std::size_t>(selected)], bytes))
			load_image_texture(bytes.data(), bytes.size(), image.id, &image.size);
	}
	return { reinterpret_cast<ImTextureID>(static_cast<intptr_t>(image.id)), image.size };
}

gui::background::import_state gui::background::status()
{
	std::lock_guard lock(background_import_mutex);
	return background_import_status;
}

void gui::background::begin_import()
{
	std::lock_guard lock(background_import_mutex);
	if (background_import_status == import_state::choosing) return;
	HMODULE reference = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		reinterpret_cast<LPCWSTR>(&choose_background_file), &reference))
	{
		background_import_status = import_state::failed;
		return;
	}
	auto job = std::make_unique<picker_job>(picker_job{reference, window, background_generation});
	background_import_status = import_state::choosing;
	const HANDLE thread = CreateThread(nullptr, 0, choose_background_file, job.get(), 0, nullptr);
	if (thread)
	{
		job.release();
		CloseHandle(thread);
	}
	else
	{
		FreeLibrary(reference);
		background_import_status = import_state::failed;
	}
}

void gui::background::update()
{
	std::vector<unsigned char> bytes;
	{
		std::lock_guard lock(background_import_mutex);
		bytes.swap(pending_background);
	}
	if (bytes.empty()) return;
	background_texture candidate;
	if (!load_image_texture(bytes.data(), bytes.size(), candidate.id, &candidate.size))
	{
		std::lock_guard lock(background_import_mutex);
		background_import_status = import_state::failed;
		return;
	}
	// Store a private copy, not a fragile path to the original. Never overwrite
	// either the source image or the last good copy on decode/write failure.
	const auto destination = custom_background_path();
	std::error_code error;
	if (!destination.empty()) std::filesystem::create_directories(destination.parent_path(), error);
	bool saved = false;
	if (!destination.empty() && !error)
	{
		const auto temporary = std::filesystem::path(destination.wstring() + L".tmp");
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			output.close();
			saved = !output.fail();
		}
		if (saved) saved = MoveFileExW(temporary.c_str(), destination.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
		if (!saved) std::filesystem::remove(temporary, error);
	}
	if (saved)
	{
		auto& previous = backgrounds[static_cast<std::size_t>(style::custom)];
		if (previous.id) glDeleteTextures(1, &previous.id);
		candidate.attempted = true;
		previous = candidate;
		gui::ui_state::get_preferences().menu_background.selected = style::custom;
		gui::ui_state::mark_preferences_dirty();
		gui::ui_state::flush_preferences(true);
	}
	else glDeleteTextures(1, &candidate.id);
	std::lock_guard lock(background_import_mutex);
	background_import_status = saved ? import_state::imported : import_state::save_failed;
}

void gui::background::shutdown()
{
	for (auto& image : backgrounds)
	{
		if (image.id) glDeleteTextures(1, &image.id);
		image = {};
	}
	std::lock_guard lock(background_import_mutex);
	++background_generation;
	pending_background.clear();
	background_import_status = import_state::idle;
}

void gui::shutdown()
{
	gui::ui_state::flush_preferences(true);
	request_shutdown.store(true, std::memory_order_release);
	while (!shut_down.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	// Give the detour epilogue time to return completely before MinHook data
	// and the DLL image can be released by the worker thread.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	MH_RemoveHook(wglSwapBuffers);
	MH_Uninitialize();
}

int gui::get_last_pressed_key()
{
	return last_pressed_key;
}

void gui::reset_last_pressed_key()
{
	last_pressed_key = 0;
}

HWND gui::get_window()
{
	return window;
}

ImColor gui::get_dynamic_color(int offset)
{
	static gui::theme::transition animation;
	const double time = ImGui::GetCurrentContext() ? ImGui::GetTime()
		: static_cast<double>(GetTickCount64()) / 1000.0;
	const auto color = animation.evaluate(active_theme, time, offset, gui::ui_state::get_preferences().reduce_motion);
	return ImColor(color.r, color.g, color.b, 1.0f);
}

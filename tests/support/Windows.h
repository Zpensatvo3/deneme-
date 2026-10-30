#pragma once
// Linux-only test include path. Never used by the DLL or a Windows build.
#ifdef _WIN32
#error Do not put the Linux GUI test shim on a Windows include path
#endif
#include <cstddef>
#include <cstdint>
inline void* SecureZeroMemory(void* destination, std::size_t length) {
    auto* bytes = static_cast<volatile unsigned char*>(destination);
    while (length--) *bytes++ = 0;
    return destination;
}
using HWND = void*;
constexpr int VK_RSHIFT = 0xA1, VK_ESCAPE = 0x1B, VK_MENU = 0x12;
constexpr int VK_LMENU = 0xA4, VK_RMENU = 0xA5, VK_SPACE = 0x20;
constexpr int VK_LBUTTON = 1, VK_RBUTTON = 2, VK_MBUTTON = 4, VK_TAB = 9;
constexpr unsigned MAPVK_VK_TO_CHAR = 2;
inline unsigned MapVirtualKeyA(unsigned key, unsigned) { return key; }
// Preference parser/normalizer tests must never access real local app data.
using DWORD = unsigned long;
inline DWORD GetEnvironmentVariableW(const wchar_t*, wchar_t*, DWORD) { return 0; }
constexpr DWORD MOVEFILE_REPLACE_EXISTING = 1, MOVEFILE_WRITE_THROUGH = 8;
inline bool MoveFileExW(const char*, const char*, DWORD) { return false; }
// Declarations for syntax-checking native bridge consumers; never executed by
// headless UI tests and never supplied to a production Windows build.
using HMODULE = void*;
using HRSRC = void*;
using HGLOBAL = void*;
using LPCWSTR = const wchar_t*;
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 4, GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 2;
inline LPCWSTR MAKEINTRESOURCEW(std::uint16_t id) { return reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(id)); }
#define RT_RCDATA MAKEINTRESOURCEW(10)
HWND GetForegroundWindow();
bool GetModuleHandleExW(DWORD, LPCWSTR, HMODULE*);
HRSRC FindResourceW(HMODULE, LPCWSTR, LPCWSTR);
DWORD SizeofResource(HMODULE, HRSRC);
HGLOBAL LoadResource(HMODULE, HRSRC);
void* LockResource(HGLOBAL);
std::uint64_t GetTickCount64();
void OutputDebugStringA(const char*);
void OutputDebugStringW(const wchar_t*);

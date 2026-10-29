#include "transport.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <array>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr wchar_t service_host[] = L"onestep-config.gogogamesas.workers.dev";
    constexpr DWORD max_response = 1024 * 1024;
    std::mutex identity_mutex;
    struct internet_handle {
        HINTERNET value = nullptr;
        explicit internet_handle(HINTERNET handle) : value(handle) {}
        ~internet_handle() { if (value) WinHttpCloseHandle(value); }
        internet_handle(const internet_handle&) = delete;
        internet_handle& operator=(const internet_handle&) = delete;
    };
    std::wstring identity_path()
    {
        const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (!size || size > 32768) return {};
        std::wstring dir(size, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", dir.data(), size);
        if (!written || written >= size) return {};
        dir.resize(written);
        dir += L"\\OneStep";
        if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
        return dir + L"\\config-owner.dat";
    }
    std::string hex(const BYTE* data, size_t length)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string out; out.reserve(length * 2);
        for (size_t i = 0; i < length; ++i) { out.push_back(digits[data[i] >> 4]); out.push_back(digits[data[i] & 15]); }
        return out;
    }
    std::string load_identity(const std::wstring& path)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 4096) { CloseHandle(file); return {}; }
        std::vector<BYTE> protected_bytes(static_cast<size_t>(length.QuadPart)); DWORD read = 0;
        const bool ok = ReadFile(file, protected_bytes.data(), static_cast<DWORD>(protected_bytes.size()), &read, nullptr) && read == protected_bytes.size();
        CloseHandle(file);
        if (!ok) return {};
        DATA_BLOB input{read, protected_bytes.data()}, output{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return {};
        std::string token = output.cbData == 32 ? hex(output.pbData, output.cbData) : std::string{};
        SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
        return token;
    }
    std::string owner_identity()
    {
        std::lock_guard lock(identity_mutex);
        const auto path = identity_path();
        if (path.empty()) return {};
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return load_identity(path);
        std::array<BYTE, 32> secret{};
        if (BCryptGenRandom(nullptr, secret.data(), static_cast<ULONG>(secret.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return {};
        DATA_BLOB input{static_cast<DWORD>(secret.size()), secret.data()}, output{};
        if (!CryptProtectData(&input, L"OneStep public config owner", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            SecureZeroMemory(secret.data(), secret.size()); return {};
        }
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, nullptr);
        if (file == INVALID_HANDLE_VALUE) { LocalFree(output.pbData); SecureZeroMemory(secret.data(), secret.size()); return load_identity(path); }
        DWORD written = 0;
        const bool ok = WriteFile(file, output.pbData, output.cbData, &written, nullptr) && written == output.cbData && FlushFileBuffers(file);
        CloseHandle(file); LocalFree(output.pbData);
        if (!ok) DeleteFileW(path.c_str()); // This file was created by this call and is incomplete.
        auto result = ok ? hex(secret.data(), secret.size()) : std::string{};
        SecureZeroMemory(secret.data(), secret.size());
        return result;
    }
}

configs::transport::service_result configs::transport::post_json(const std::string& path, const std::string& fields)
{
    service_result result;
    const auto owner = owner_identity();
    if (owner.empty()) { result.message = "Could not access the config identity."; return result; }
    if (path != "/api/v1/configs/list" && path != "/api/v1/configs/load" && path != "/api/v1/configs/publish" && path != "/api/v1/configs/delete") return result;
    const std::string body = "{\"owner_token\":\"" + owner + "\"" + (fields.empty() ? "" : "," + fields) + "}";
    if (body.size() > 512 * 1024) { result.message = "Config is too large."; return result; }
    internet_handle session(WinHttpOpen(L"OneStep-Config/1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value) { result.message = "Could not start network request."; return result; }
    WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 5000);
    internet_handle connection(WinHttpConnect(session.value, service_host, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value) { result.message = "Could not connect to config service."; return result; }
    const std::wstring wide_path(path.begin(), path.end());
    internet_handle request(WinHttpOpenRequest(connection.value, L"POST", wide_path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) { result.message = "Could not create config request."; return result; }
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSendRequest(request.value, L"Content-Type: application/json\r\n", -1, const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) { result.message = "Config service is unavailable."; return result; }
    DWORD status = 0, status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) return result;
    result.status_code = static_cast<int>(status);
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) { result.message = "Config response failed."; return result; }
        if (!available) break;
        if (available > max_response - result.body.size()) { result.message = "Config response is too large."; return result; }
        const auto offset = result.body.size(); result.body.resize(offset + available); DWORD received = 0;
        if (!WinHttpReadData(request.value, result.body.data() + offset, available, &received) || !received) { result.message = "Config response failed."; return result; }
        result.body.resize(offset + received);
    }
    result.success = status >= 200 && status < 300;
    if (!result.success) result.message = "Config request failed (HTTP " + std::to_string(status) + ").";
    return result;
}

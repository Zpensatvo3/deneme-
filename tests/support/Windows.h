#pragma once
// Linux-only test include path. Never used by the DLL or a Windows build.
#ifdef _WIN32
#error Do not put the Linux GUI test shim on a Windows include path
#endif
#include <cstddef>
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

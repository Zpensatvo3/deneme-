#include <Windows.h>
#include "app/runtime.hpp"

namespace
{
    DWORD WINAPI worker_entry(void* context)
    {
        app::run();
        // run() returns after cache/JNI/session destructors have completed.
        FreeLibraryAndExitThread(static_cast<HMODULE>(context), 0);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE library, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(library);
    const HANDLE worker = CreateThread(nullptr, 0, worker_entry, library, 0, nullptr);
    if (!worker) return FALSE;
    CloseHandle(worker);
    return TRUE;
}

#pragma once

// A local test DLL must leave evidence for CRT aborts; the Minecraft Java log
// otherwise ends at "Stopping!" without a native stack. Preserve CRT behavior.
#if defined(ONESTEP_LOCAL_TEST_BUILD) && ONESTEP_LOCAL_TEST_BUILD == 1
#include <Windows.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace platform
{
    class test_crash_report
    {
        using signal_handler = void(*)(int);
        inline static std::terminate_handler previous_terminate = nullptr;
        signal_handler previous_abort = SIG_DFL;

        static void record(const char* reason) noexcept
        {
            char directory[MAX_PATH]{}, path[MAX_PATH]{};
            const auto length=GetTempPathA(MAX_PATH,directory);
            if (!length || length>=MAX_PATH-40) return;
            if (sprintf_s(path,"%sOneStep-abort-%lu.log",directory,GetCurrentProcessId())<0) return;
            const auto file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if (file==INVALID_HANDLE_VALUE) return;
            char line[1024]{}; DWORD written=0;
            SYSTEMTIME time{}; GetSystemTime(&time);
            auto size=sprintf_s(line,"%04u-%02u-%02u %02u:%02u:%02u UTC | %s | thread %lu\r\n",
                time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,reason,GetCurrentThreadId());
            if (size>0) WriteFile(file,line,static_cast<DWORD>(size),&written,nullptr);
            void* frames[48]{};
            const auto count=CaptureStackBackTrace(0,48,frames,nullptr);
            for (USHORT i=0;i<count;++i) {
                HMODULE module=nullptr; char module_path[MAX_PATH]{};
                const bool found=GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(frames[i]),&module)!=FALSE;
                if (found) GetModuleFileNameA(module,module_path,MAX_PATH);
                const auto offset=reinterpret_cast<ULONG_PTR>(frames[i])-reinterpret_cast<ULONG_PTR>(module);
                size=sprintf_s(line,"%02u %p %s + 0x%llx\r\n",i,frames[i],found?module_path:"<unknown>",static_cast<unsigned long long>(offset));
                if (size>0) WriteFile(file,line,static_cast<DWORD>(size),&written,nullptr);
            }
            FlushFileBuffers(file); CloseHandle(file);
        }
        static void on_abort(int) noexcept { record("SIGABRT"); }
        static void on_terminate() noexcept
        {
            record("std::terminate");
            if (previous_terminate && previous_terminate!=&on_terminate) previous_terminate();
            std::abort();
        }
    public:
        test_crash_report() noexcept
        {
            previous_abort=std::signal(SIGABRT,&on_abort);
            previous_terminate=std::set_terminate(&on_terminate);
        }
        ~test_crash_report()
        {
            std::set_terminate(previous_terminate);
            if (previous_abort!=SIG_ERR) std::signal(SIGABRT,previous_abort);
        }
        test_crash_report(const test_crash_report&)=delete;
        test_crash_report& operator=(const test_crash_report&)=delete;
    };
}
#else
namespace platform { struct test_crash_report {}; }
#endif

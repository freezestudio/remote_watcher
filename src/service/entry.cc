//
// dll service entry
//

#include "dep.h"

#define SERVICE_NAME L"mnsvc"

SERVICE_STATUS_HANDLE ss_handle = nullptr;

DWORD __stdcall handler_proc(
    DWORD    dwControl,
    DWORD    dwEventType,
    LPVOID   lpEventData,
    LPVOID   lpContext
    );
void register_handler();
void init_threadpool();

void __stdcall entry(DWORD argc, LPWSTR * argv)
{
    register_handler();
}

void register_handler()
{
    ss_handle = ::RegisterServiceCtrlHandlerExW(SERVICE_NAME, handler_proc, nullptr);
}

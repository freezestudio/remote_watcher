//
// dll service entry
//

#include "dep.h"
#include "sdep.h"

#define SERVICE_NAME L"rgmsvc"

SERVICE_STATUS_HANDLE ss_handle = nullptr;

DWORD __stdcall handler_proc(
    DWORD dwControl,
    DWORD dwEventType,
    LPVOID lpEventData,
    LPVOID lpContext);
void register_handler();
void init_threadpool();

/**
 * @brief 服务入口
 * 预期由 svchost.exe 调用, 服务的初始状态为 SERVICE_STOPPED
 * @param argc 参数个数
 * @param argv 参数列表
 */
void __stdcall entry(DWORD argc, LPWSTR *argv)
{
    // 初始化所有全局变量
    // 如果初始化时间不超过 1s, 可以直接设置服务状态为 SERVICE_RUNNING

    register_handler();
}

void register_handler()
{
    ss_handle = ::RegisterServiceCtrlHandlerExW(SERVICE_NAME, handler_proc, nullptr);
}

DWORD __stdcall handler_proc(
    DWORD dwControl,
    DWORD dwEventType,
    LPVOID lpEventData,
    LPVOID lpContext)
{
    return NO_ERROR;
}

void init_threadpool()
{
    SERVICE_WIN32;
}

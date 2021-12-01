//
// dll service entry
//

#include "dep.h"
#include "sdep.h"

SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable = nullptr;

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
void __stdcall ServiceMain(DWORD argc, LPWSTR* argv)
{
	// 初始化所有全局变量
	// 如果初始化时间不超过 1s, 可以直接设置服务状态为 SERVICE_RUNNING

	if (argc > 0)
	{
		auto _str_ip = argv[0];
		auto _msg = std::format(L"remote ip is: {}\n", _str_ip);
		OutputDebugString(_msg.data());
	}


	register_handler();
}

void register_handler()
{
	DWORD check_point = 0;
	ss_handle = ::RegisterServiceCtrlHandlerExW(SERVICE_NAME, handler_proc, nullptr);
	if (!ss_handle)
	{
		OutputDebugStringA("register service control handler failure, exit.\n");
		return;
	}

	// 初始化服务状态
	SERVICE_STATUS status{};
	status.dwServiceType = SERVICE_WIN32; // 服务类型
	status.dwCurrentState = SERVICE_START_PENDING; // 等待启动
	status.dwControlsAccepted = 0; // 等待期间不接受控制
	status.dwWaitHint = 3000; // 等待3s
	status.dwCheckPoint = 0;
	::SetServiceStatus(ss_handle, &status);

	hh_waitable = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable)
	{
		OutputDebugStringA("create waitable handle failure, exit.\n");

		status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
		status.dwWaitHint = 30000; // 等待 30s
		status.dwCheckPoint = 0;
		status.dwWin32ExitCode = 0;
		::SetServiceStatus(ss_handle, &status);

		return;
	}

	// start multi threads
	// ...

	// 运行服务
	check_point++;
	status.dwCurrentState = SERVICE_START; // 启动
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; // 接受这些控制
	status.dwCheckPoint = check_point;
	status.dwWin32ExitCode = 0;
	status.dwServiceSpecificExitCode = 0;
	status.dwWaitHint = 0;
	::SetServiceStatus(ss_handle, &status);

	// loop here
	while (true)
	{
		// dwMilliseconds: 设置为 INFINITE -> 仅允许(object-signaled | I/O completion routine | APC)
		// bAlertable: 需要 I/O completion routine 或 QueueUserAPC
		// res:
		//   WAIT_ABANDONED: 未释放的锁
		//   WAIT_IO_COMPLETION
		//   WAIT_OBJECT_0
		//   WAIT_TIMEOUT
		//   WAIT_FAILED   
		//auto res = ::WaitForSingleObjectEx(hh_waitable, INFINITE, TRUE);
		auto res = ::WaitForSingleObject(hh_waitable, INFINITE);

		// ignore WAIT_TIMEOUT

		if (res == WAIT_OBJECT_0)
		{
			OutputDebugStringA("waitable handle signed: WAIT_OBJECT_0\n");

			// stop service
			// ...

			break;
		}
		else if (res == WAIT_IO_COMPLETION)
		{
			OutputDebugStringA("i/o completion routine: WAIT_IO_COMPLETION\n");
			// continue...
		}
		else if (res == WAIT_ABANDONED)
		{
			// error, a thread terminated. but not release mutex object.
			OutputDebugStringA("mutex object not released: WAIT_ABANDONED\n");
		}
		else if (res == WAIT_FAILED)
		{
			auto err = ::GetLastError();
			auto _msg = std::format("WaitForSingleObject: occuse an error: {}\n", err);
			OutputDebugStringA(_msg.c_str());
		}
	}

	status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
	status.dwWaitHint = 30000; // 等待 30s
	status.dwCheckPoint = 0;
	status.dwWin32ExitCode = 0;
	::SetServiceStatus(ss_handle, &status);

	OutputDebugStringA("service:rgmsvc stopped.\n");
}

DWORD __stdcall handler_proc(
	DWORD dwControl,
	DWORD dwEventType,
	LPVOID lpEventData,
	LPVOID lpContext)
{
	switch (dwControl)
	{
	default:
		break;
	case SERVICE_STOP:
		break;
	}
	return NO_ERROR;
}

void init_threadpool()
{
	SERVICE_WIN32;
}

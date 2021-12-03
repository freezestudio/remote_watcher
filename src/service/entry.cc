//
// dll service entry
//

#include "dep.h"
#include "sdep.h"

SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable = nullptr;

// deprecated, test only!
HANDLE hh_mockthread = nullptr;
// deprecated, test only!
bool bb_mockthread_exit = false;

DWORD __stdcall handler_proc(DWORD dwControl, DWORD, LPVOID, LPVOID);
void register_handler();
void init_threadpool();

// deprecated, test only!
DWORD __stdcall _MockThread(LPVOID);

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
		auto _msg = std::format(L"@rg remote ip is: {}\n", _str_ip);
		OutputDebugString(_msg.data());
	}

	register_handler();
}

// deprecated. test only!
DWORD __stdcall _MockThread(LPVOID)
{
	while (!bb_mockthread_exit)
	{
		Sleep(5000);
		OutputDebugStringA("@rg Service Work Thread running over 5s ...");
	}
	return 0;
}

void register_handler()
{
	DWORD check_point = 0;
	ss_handle = ::RegisterServiceCtrlHandlerExW(SERVICE_NAME, handler_proc, nullptr);
	if (!ss_handle)
	{
		OutputDebugStringA("@rg register service control handler failure, exit.\n");
		return;
	}

	// first init sevice status
	SERVICE_STATUS status{};
	status.dwServiceType = SERVICE_WIN32; // service type
	status.dwCurrentState = SERVICE_START_PENDING; // wait start
	status.dwControlsAccepted = 0; // 等待期间不接受控制
	status.dwWaitHint = 3000; // wait 3s
	status.dwCheckPoint = 0;
	::SetServiceStatus(ss_handle, &status);

	hh_waitable = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable)
	{
		OutputDebugStringA("@rg create waitable handle failure, exit.\n");

		status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
		status.dwWaitHint = 30000; // wait 30s
		status.dwCheckPoint = 0;
		status.dwWin32ExitCode = 0;
		::SetServiceStatus(ss_handle, &status);

		return;
	}
	bb_mockthread_exit = false;

	// start multi threads
	// ...

	hh_mockthread = ::CreateThread(nullptr, 0, _MockThread, nullptr, 0, nullptr);
	if (!hh_mockthread)
	{
		OutputDebugStringA("@rg Mock Thread failure.\n");
		goto theend;
	}

	// 运行服务
	check_point++;
	status.dwCurrentState = SERVICE_START; // started
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; // accept[stop, shutdown]
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
			OutputDebugStringA("@rg waitable handle signed: WAIT_OBJECT_0\n");

			// stop service
			// ...

			break;
		}
		else if (res == WAIT_IO_COMPLETION)
		{
			OutputDebugStringA("@rg i/o completion routine: WAIT_IO_COMPLETION\n");
			// continue...
		}
		else if (res == WAIT_ABANDONED)
		{
			// error, a thread terminated. but not release mutex object.
			OutputDebugStringA("@rg mutex object not released: WAIT_ABANDONED\n");
		}
		else if (res == WAIT_FAILED)
		{
			auto err = ::GetLastError();
			auto _msg = std::format("@rg WaitForSingleObject: occuse an error: {}\n", err);
			OutputDebugStringA(_msg.c_str());
		}
	}

theend:
	bb_mockthread_exit = true;

	status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
	status.dwWaitHint = 30000; // 等待 30s
	status.dwCheckPoint = 0;
	status.dwWin32ExitCode = 0;
	::SetServiceStatus(ss_handle, &status);

	::CloseHandle(hh_mockthread);
	::CloseHandle(hh_waitable);
	OutputDebugStringA("@rg service:rgmsvc stopped.\n");
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
		bb_mockthread_exit = true;
		SetEvent(hh_waitable);
		OutputDebugStringA("@rg Recv [stop] control code.\n");
		break;
	}
	return NO_ERROR;
}

void init_threadpool()
{
	SERVICE_WIN32;
}

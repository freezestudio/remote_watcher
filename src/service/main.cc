//
// exe service entry
//

#include "dep.h"
#include "sdep.h"

using namespace std::literals;

SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable = nullptr;

// deprecated, test only!
HANDLE hh_mockthread = nullptr;
// deprecated, test only!
bool bb_mockthread_exit = false;
std::wstring ss_ip;

void init_service();
bool init_threadpool();

DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);
// deprecated, test only!
DWORD __stdcall _MockThread(LPVOID);

/**
 * @brief 服务入口
 * @param argc 参数个数
 * @param argv 参数列表
 */
void __stdcall ServiceMain(DWORD argc, LPWSTR* argv)
{
	// 初始化所有全局变量
	// 如果初始化时间不超过 1s, 可以直接设置服务状态为 SERVICE_RUNNING

	if (argc > 1)
	{
		ss_ip = argv[1];
		auto _msg = std::format(L"@rg remote ip is: {}\n"sv, ss_ip);
		OutputDebugString(_msg.data());
	}
	OutputDebugString(L"@rg rgmsvc starting ...\n");
	init_service();
	OutputDebugString(L"@rg remsvc stopped.\n");
}

// deprecated. test only!
DWORD __stdcall _MockThread(LPVOID)
{
	while (!bb_mockthread_exit)
	{
		Sleep(5000);
		OutputDebugString(L"@rg Service Work Thread running over 5s ...");
	}

	return 0;
}

void init_service()
{
	DWORD check_point = 0;
	ss_handle = ::RegisterServiceCtrlHandlerEx(SERVICE_NAME, handler_proc_ex, nullptr);
	if (!ss_handle)
	{
		OutputDebugString(L"@rg register service control handler failure, exit.\n");
		return;
	}
	OutputDebugString(L"@rg service control handler registered.\n");

	// first init sevice status
	// state=[SERVICE_START_PENDING]: dwControlsAcceptd=0
	// state=[SERVICE_RUNNING|SERVICE_STOPPED]: dwCheckPoint=0
	// 
	SERVICE_STATUS status{};
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // service type
	status.dwCurrentState = SERVICE_START_PENDING; // wait start
	status.dwControlsAccepted = 0; // 等待期间不接受控制
	status.dwWaitHint = 3000; // wait 3s
	status.dwCheckPoint = 0;
	status.dwWin32ExitCode = 0;
	status.dwServiceSpecificExitCode = 0;
	::SetServiceStatus(ss_handle, &status);
	OutputDebugString(L"@rg service start pending 3s.\n");

	hh_waitable = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable)
	{
		OutputDebugString(L"@rg create waitable handle failure, exit.\n");

		status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		status.dwWaitHint = 30000; // wait 30s
		status.dwCheckPoint = ++check_point;
		status.dwWin32ExitCode = 0;
		::SetServiceStatus(ss_handle, &status);
		OutputDebugString(L"@rg service CreateEvent Failure. stop pending.\n");
		return;
	}

	// start multi threads
	if (!init_threadpool())
	{
		goto theend;
	}

	// service running
	status.dwCurrentState = SERVICE_RUNNING; // started
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; // accept[stop, shutdown]
	status.dwCheckPoint = 0;
	status.dwWin32ExitCode = 0;
	status.dwServiceSpecificExitCode = 0;
	status.dwWaitHint = 0;
	::SetServiceStatus(ss_handle, &status);
	OutputDebugString(L"@rg service starting ...\n");

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
			OutputDebugString(L"@rg waitable handle signed: WAIT_OBJECT_0\n");

			// stop service
			// ...
		}
		else if (res == WAIT_IO_COMPLETION)
		{
			OutputDebugString(L"@rg i/o completion routine: WAIT_IO_COMPLETION\n");
			// continue...
		}
		else if (res == WAIT_ABANDONED)
		{
			// error, a thread terminated. but not release mutex object.
			OutputDebugString(L"@rg mutex object not released: WAIT_ABANDONED\n");
		}
		else if (res == WAIT_FAILED)
		{
			auto err = ::GetLastError();
			auto _msg = std::format(L"@rg WaitForSingleObject: occuse an error: {}\n"sv, err);
			OutputDebugString(_msg.c_str());
		}

		// here, all break?
		break;
	}

theend:
	status.dwCurrentState = SERVICE_STOP_PENDING; // 等待停止
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	status.dwWaitHint = 30000; // 等待 30s
	status.dwCheckPoint = ++check_point;
	status.dwWin32ExitCode = 0;
	::SetServiceStatus(ss_handle, &status);

	if (hh_mockthread)
	{
		::CloseHandle(hh_mockthread);
		hh_mockthread = nullptr;
	}

	if (hh_waitable)
	{
		::CloseHandle(hh_waitable);
		hh_waitable = nullptr;
	}

	OutputDebugString(L"@rg service:rgmsvc stopped.\n");
}

DWORD __stdcall handler_proc_ex(
	DWORD dwControl,
	DWORD dwEventType,
	LPVOID lpEventData,
	LPVOID lpContext)
{
	switch (dwControl)
	{
	default:
		break;
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		bb_mockthread_exit = true;
		SetEvent(hh_waitable);
		OutputDebugString(L"@rg Recv [stop] control code.\n");
		break;
	}
	return NO_ERROR;
}

bool init_threadpool()
{
	bb_mockthread_exit = false;
	hh_mockthread = ::CreateThread(nullptr, 0, _MockThread, nullptr, 0, nullptr);
	if (!hh_mockthread)
	{
		OutputDebugString(L"@rg servic create Mock Thread failure.\n");
		bb_mockthread_exit = true;
		return false;
	}
	return true;
}

int __stdcall wmain()
{
	wchar_t _service_name[] = SERVICE_NAME;
	SERVICE_TABLE_ENTRY _dispatch_table[] = {
		{_service_name, static_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
		{nullptr, nullptr},
	};
	if (StartServiceCtrlDispatcher(_dispatch_table))
	{
		OutputDebugString(L"Dispatch Service Successfully.\n");
	}
	else
	{
		OutputDebugString(L"Dispatch Service Failure.\n");
	}
}

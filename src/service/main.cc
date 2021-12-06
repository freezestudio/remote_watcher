//
// exe service entry
//

#include "dep.h"
#include "sdep.h"

using namespace std::literals;

SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable = nullptr;
DWORD cp_check_point = 0;

// deprecated, test only!
HANDLE hh_mockthread = nullptr;
// deprecated, test only!
bool bb_mockthread_exit = false;
bool bb_mockthread_run = false;

std::wstring ss_ip;

void init_service();
bool init_threadpool();
bool update_status(SERVICE_STATUS_HANDLE, DWORD, DWORD = NO_ERROR);

DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);
// deprecated, test only!
DWORD __stdcall _MockThread(LPVOID);

// deprecated. test only!
DWORD __stdcall _MockThread(LPVOID)
{
	while (!bb_mockthread_exit)
	{
		// working ...
		Sleep(5000);

		if (bb_mockthread_run)
		{
			OutputDebugString(L"@rg Service Work Thread running over 5s ...");
		}
		else
		{
			OutputDebugString(L"@rg Service Work Thread paused.");
		}
	}

	return 0;
}

void init_service()
{
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
	update_status(ss_handle, SERVICE_START_PENDING);
	OutputDebugString(L"@rg service start pending 3s.\n");

	hh_waitable = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable)
	{
		OutputDebugString(L"@rg create waitable handle failure, exit.\n");

		update_status(ss_handle, SERVICE_STOP_PENDING);
		OutputDebugString(L"@rg service CreateEvent Failure. stop pending.\n");
		return;
	}

	// start multi threads
	if (!init_threadpool())
	{
		goto theend;
	}

	// service running
	update_status(ss_handle, SERVICE_RUNNING);
	bb_mockthread_run = true;
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
	// stop service
	update_status(ss_handle, SERVICE_STOPPED);

	cp_check_point = 0;
	bb_mockthread_exit = true;
	bb_mockthread_run = false;

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
	case SERVICE_CONTROL_PAUSE:
		// pause service
		OutputDebugString(L"@rg Recv [pause] control code.\n");
		update_status(ss_handle, SERVICE_PAUSE_PENDING);
		bb_mockthread_run = false;
		OutputDebugString(L"@rg Recv [pause] control code: dosomething ...\n");
		update_status(ss_handle, SERVICE_PAUSED);
		OutputDebugString(L"@rg Recv [pause] control code: Service Paused.\n");
		break;
	case SERVICE_CONTROL_CONTINUE:
		// resume service
		OutputDebugString(L"@rg Recv [resume] control code.\n");
		update_status(ss_handle, SERVICE_CONTINUE_PENDING);
		bb_mockthread_run = true;
		OutputDebugString(L"@rg Recv [resume] control code: dosomething...\n");
		update_status(ss_handle, SERVICE_RUNNING);
		OutputDebugString(L"@rg Recv [resume] control code: Service Running.\n");
		break;
	case SERVICE_CONTROL_INTERROGATE:
		// report current status, should simply return NO_ERROR
		break;
	case SERVICE_CONTROL_PARAMCHANGE:
		// startup parameters changed
		break;
		// case SERVICE_CONTROL_PRESHUTDOWN:
		// pre-shutdown
		// break;
	case SERVICE_CONTROL_SHUTDOWN:
		// shutdown, return NO_ERROR;
		// break;
	case SERVICE_CONTROL_STOP:
		// stop service, eturn NO_ERROR;
		bb_mockthread_exit = true;
		bb_mockthread_run = false;
		SetEvent(hh_waitable);
		OutputDebugString(L"@rg Recv [stop] control code.\n");
		update_status(ss_handle, SERVICE_STOP_PENDING);
		// SERVICE_STOPPED in init_service()
		break;
	case SERVICE_CONTROL_NETWORK_CONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		break;
	case SERVICE_CONTROL_NETWORK_DISCONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		break;
	default:
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
		OutputDebugString(L"@rg service create Mock Thread failure.\n");
		bb_mockthread_exit = true;
		bb_mockthread_run = false;
		return false;
	}
	return true;
}

bool update_status(SERVICE_STATUS_HANDLE hss, DWORD state, DWORD error_code)
{
	if (!hss)
	{
		return false;
	}

	SERVICE_STATUS service_status{
		.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
		.dwServiceSpecificExitCode = 0,
	};

	service_status.dwCurrentState = state;
	service_status.dwWin32ExitCode = error_code;
	if (service_status.dwCurrentState == SERVICE_START_PENDING)
	{
		service_status.dwControlsAccepted = 0;
	}
	else
	{
		service_status.dwControlsAccepted =
			SERVICE_ACCEPT_PARAMCHANGE |
			SERVICE_ACCEPT_PAUSE_CONTINUE |
			SERVICE_ACCEPT_SHUTDOWN |
			SERVICE_ACCEPT_STOP;
	}

	// maybe need add wait-hint to SERVICE_PAUSED
	// if (service_status.dwCurrentState == SERVICE_CONTINUE_PENDING ||
	// 	service_status.dwCurrentState == SERVICE_PAUSE_PENDING ||
	// 	service_status.dwCurrentState == SERVICE_START_PENDING ||
	// 	service_status.dwCurrentState == SERVICE_STOP_PENDING)
	// {
	// 	service_status.dwCheckPoint = cp_check_point++;
	// 	service_status.dwWaitHint = 3000;
	// }
	// else
	// {
	// 	service_status.dwCheckPoint = 0;
	// 	service_status.dwWaitHint = 0;
	// }

	if(service_status.dwCurrentState == SERVICE_RUNNING ||
		service_status.dwCurrentState == SERVICE_STOPPED)
	{
		service_status.dwCheckPoint = 0;
		service_status.dwWaitHint = 0;
	}
	else
	{
		service_status.dwCheckPoint = cp_check_point++;
		service_status.dwWaitHint = 3000;
	}

	auto ok = SetServiceStatus(hss, &service_status) ? true : false;
	return ok;
}

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
		auto _msg = std::format(L"@rg ServiceMain: argc={}, RemoteIP is: {}\n"sv, argc, ss_ip);
		OutputDebugString(_msg.data());
	}
	for (auto i = 0; i < argc; ++i)
	{
		auto _msg = std::format(L"@rg ServiceMain: argc[{}], {}\n"sv, i, argv[i]);
		OutputDebugString(_msg.c_str());
	}

	OutputDebugString(L"@rg ServiceMain: rgmsvc starting ...\n");
	init_service();
	OutputDebugString(L"@rg ServiceMain: rgmsvc stopped.\n");
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
		OutputDebugString(L"@rg main: Dispatch Service Successfully.\n");
	}
	else
	{
		OutputDebugString(L"@rg main: Dispatch Service Failure.\n");
	}
}

//
// TODO: add rundll32.exe interface %ProgramFiles% 改为为 c:\progra~1
// TODO: rundll32.exe rgmsvc.exe, Install 192.168.0.1
//

VOID __stdcall Install(HWND hWnd, HINSTANCE hInst, LPWSTR lpCmdLine, int nCmdShow)
{

}

VOID __stdcall Uninstall(HWND hWnd, HINSTANCE hInst, LPWSTR lpCmdLine, int nCmdShow)
{

}

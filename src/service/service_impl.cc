#include "service.h"

// service status handle.
SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable_event = nullptr;
DWORD cp_check_point = 0;
freeze::service_state ss_current_status = freeze::service_state::stopped;

bool init_service()
{
#ifndef SERVICE_TEST
	ss_handle = ::RegisterServiceCtrlHandlerEx(SERVICE_NAME, handler_proc_ex, nullptr);
	if (!ss_handle)
	{
		DEBUG_STRING(L"@rg register service control handler failure, exit.\n");
		return false;
	}
	DEBUG_STRING(L"@rg service control handler registered.\n");

	// first init sevice status
	// state=[SERVICE_START_PENDING]: dwControlsAcceptd=0
	// state=[SERVICE_RUNNING|SERVICE_STOPPED]: dwCheckPoint=0
	// 
	update_status(ss_handle, SERVICE_START_PENDING);
	DEBUG_STRING(L"@rg service start pending 3s.\n");
#endif

	hh_waitable_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable_event)
	{
		DEBUG_STRING(L"@rg create waitable handle failure, exit.\n");

		update_status(ss_handle, SERVICE_STOP_PENDING);
		DEBUG_STRING(L"@rg service CreateEvent Failure. stop pending.\n");
		return false;
	}
    return true;
}

void run_service()
{
#ifndef SERVICE_TEST
	// service running
	update_status(ss_handle, SERVICE_RUNNING);
	DEBUG_STRING(L"@rg service starting ...\n");
#endif

	// loop here
	while (true)
	{
		// dwMilliseconds: 设置为 INFINITE -> 仅允许(object-signaled | I/O completion routine | APC)
		// bAlertable: 需要 I/O completion routine 或 QueueUserAPC
		//auto res = ::WaitForSingleObjectEx(hh_waitable_event, INFINITE, TRUE);
		// res:
		//   WAIT_ABANDONED: 未释放的锁
		//   WAIT_IO_COMPLETION
		//   WAIT_OBJECT_0
		//   WAIT_TIMEOUT
		//   WAIT_FAILED   
		auto res = ::WaitForSingleObject(hh_waitable_event, INFINITE);

		// ignore WAIT_TIMEOUT
		if (res == WAIT_OBJECT_0)
		{
			DEBUG_STRING(L"@rg waitable handle signed: WAIT_OBJECT_0\n");

			// stop service
			// ...
		}
		else if (res == WAIT_IO_COMPLETION)
		{
			DEBUG_STRING(L"@rg i/o completion routine: WAIT_IO_COMPLETION\n");
			// continue...
		}
		else if (res == WAIT_ABANDONED)
		{
			// error, a thread terminated. but not release mutex object.
			DEBUG_STRING(L"@rg mutex object not released: WAIT_ABANDONED\n");
		}
		else if (res == WAIT_FAILED)
		{
			auto err = ::GetLastError();
			DEBUG_STRING(L"@rg WaitForSingleObject: WAIT_FAILED: {}\n"sv, err);
		}
		else
		{
			auto err = ::GetLastError();
			DEBUG_STRING(L"@rg WaitForSingleObject: occuse an error: {}\n"sv, err);
		}

		// here, all break?
		break;
	}
}

void stop_service()
{
#ifndef SERVICE_TEST
	// stop service
	update_status(ss_handle, SERVICE_STOPPED);
#endif

	cp_check_point = 0;	
	if (hh_waitable_event)
	{
		::CloseHandle(hh_waitable_event);
		hh_waitable_event = nullptr;
	}

	DEBUG_STRING(L"@rg service:rgmsvc stopped.\n");
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
		DEBUG_STRING(L"@rg Recv [pause] control code.\n");
		update_status(ss_handle, SERVICE_PAUSE_PENDING);
		ss_current_status = freeze::service_state::pause_pending;
		DEBUG_STRING(L"@rg Recv [pause] control code: dosomething ...\n");
		update_status(ss_handle, SERVICE_PAUSED);
		ss_current_status = freeze::service_state::paused;
		DEBUG_STRING(L"@rg Recv [pause] control code: Service Paused.\n");
		break;
	case SERVICE_CONTROL_CONTINUE:
		// resume service
		DEBUG_STRING(L"@rg Recv [resume] control code.\n");
		update_status(ss_handle, SERVICE_CONTINUE_PENDING);
		ss_current_status = freeze::service_state::continue_pending;
		DEBUG_STRING(L"@rg Recv [resume] control code: dosomething...\n");
		update_status(ss_handle, SERVICE_RUNNING);
		ss_current_status = freeze::service_state::running;
		DEBUG_STRING(L"@rg Recv [resume] control code: Service Running.\n");
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
		SetEvent(hh_waitable_event);
		DEBUG_STRING(L"@rg Recv [stop] control code.\n");
		update_status(ss_handle, SERVICE_STOP_PENDING);
		ss_current_status = freeze::service_state::stop_pending;
		// SERVICE_STOPPED in init_service()
		break;
	case SERVICE_CONTROL_NETWORK_CONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		ss_current_status = freeze::service_state::network_connect;
		break;
	case SERVICE_CONTROL_NETWORK_DISCONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		ss_current_status = freeze::service_state::network_disconnect;
		break;
	default:
		break;
	}
	return NO_ERROR;
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

	if (service_status.dwCurrentState == SERVICE_RUNNING ||
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

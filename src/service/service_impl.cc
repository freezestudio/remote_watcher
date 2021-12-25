//
// service implement
//

#include "service.h"
#include "service_extern.h"

// service status handle.
SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable_event = nullptr;
DWORD cp_check_point = 0;

#ifndef SERVICE_TEST
/* extern */
freeze::service_state ss_current_status = freeze::service_state::stopped;
freeze::service_state get_service_status()
{
	return ss_current_status;
}
void set_service_status(DWORD status)
{
	ss_current_status = to_enum<freeze::service_state>(status);
}
bool is_service_running()
{
	return ss_current_status == freeze::service_state::running;
}
#endif

bool init_service()
{
#ifndef SERVICE_TEST
	ss_handle = ::RegisterServiceCtrlHandlerEx(SERVICE_NAME, handler_proc_ex, nullptr);
	if (!ss_handle)
	{
		DEBUG_STRING(L"@rg Service: Register ControlHandler failure, exit.\n");
		return false;
	}
	DEBUG_STRING(L"@rg Service: ControlHandler Register Successfully.\n");
		
	update_status(ss_handle, SERVICE_START_PENDING);
	DEBUG_STRING(L"@rg Service: Start Pending 30s.\n");
#endif

	hh_waitable_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable_event)
	{
		DEBUG_STRING(L"@rg Service: Create waitable handle failure, exit.\n");

		update_status(ss_handle, SERVICE_STOP_PENDING);
		DEBUG_STRING(L"@rg Service: CreateEvent Failure. Stop Pending...\n");
		return false;
	}
    return true;
}

void run_service()
{
#ifndef SERVICE_TEST
	// service running
	update_status(ss_handle, SERVICE_RUNNING);
	DEBUG_STRING(L"@rg Service: Started.\n");
#endif

	while (true)
	{
		::WaitForSingleObject(hh_waitable_event, INFINITE);
		break;
	}
	DEBUG_STRING(L"@rg Service: Stopping ...\n");
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

	DEBUG_STRING(L"@rg Service: Service stopped.\n");
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
		DEBUG_STRING(L"@rg Recv [pause] control code: do-something ...\n");
		update_status(ss_handle, SERVICE_PAUSED);
		DEBUG_STRING(L"@rg Recv [pause] control code: Service Paused.\n");
		break;
	case SERVICE_CONTROL_CONTINUE:
		// resume service
		DEBUG_STRING(L"@rg Recv [resume] control code.\n");
		update_status(ss_handle, SERVICE_CONTINUE_PENDING);
		DEBUG_STRING(L"@rg Recv [resume] control code: do-something...\n");
		update_status(ss_handle, SERVICE_RUNNING);
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
		[[fallthrough]];
	case SERVICE_CONTROL_STOP:
		// stop service, eturn NO_ERROR;
		SetEvent(hh_waitable_event);
		DEBUG_STRING(L"@rg Recv [stop] control code.\n");
		update_status(ss_handle, SERVICE_STOP_PENDING);
		// SERVICE_STOPPED
		break;
	case SERVICE_CONTROL_NETWORK_CONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		update_status(ss_handle, SERVICE_CONTROL_NETWORK_CONNECT);
		break;
	case SERVICE_CONTROL_NETWORK_DISCONNECT:
		// return ERROR_CALL_NOT_IMPLEMENTED;
		update_status(ss_handle, SERVICE_CONTROL_NETWORK_DISCONNECT);
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

	SERVICE_STATUS service_status;
	service_status.dwServiceType = SERVICE_WIN32;
	service_status.dwServiceSpecificExitCode = 0;

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

	if (service_status.dwCurrentState == SERVICE_RUNNING ||
		service_status.dwCurrentState == SERVICE_STOPPED)
	{
		service_status.dwCheckPoint = 0;
		service_status.dwWaitHint = 0;
	}
	else
	{
		service_status.dwCheckPoint = cp_check_point++;
		service_status.dwWaitHint = 30000;
	}

	auto ok = SetServiceStatus(hss, &service_status) ? true : false;
#ifndef SERVICE_TEST
	if(ok)
	{
		set_service_status(state);
	}
#endif
	return ok;
}

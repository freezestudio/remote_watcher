//
// exe service entry
//

#include "dep.h"
#include "sdep.h"

#define TIMER_SECOND 10000000

//
// TODO: #include <netlistmgr.h>
// com: INetworkConnection::get_IsConnected
//

using namespace std::literals;

SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable_event = nullptr;
DWORD cp_check_point = 0;

// work thread handle
HANDLE hh_worker_thread = nullptr;
// work thread status
bool bb_worker_thread_exit = false;
bool bb_worker_thread_run = false;

// timer thread handle
HANDLE hh_timer_thread = nullptr;
HANDLE hh_timer = nullptr;

std::wstring ss_ip;

void init_service();
bool init_threadpool();
bool update_status(SERVICE_STATUS_HANDLE, DWORD, DWORD = NO_ERROR);

DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID);
DWORD __stdcall _TimerThread(LPVOID);
// timer thread callback
void __stdcall _TimerCallback(LPVOID, DWORD, DWORD);

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID)
{
	// Alertable Thread

	// watch folder, recv folder-notify, emit to nats
	// watcher.on_data([](auto&& image){
	//     send_to(nats_server, image);
	// });
	// recv message from nats
	// nats_ptr->on_msg([](auto&& msg){});
	// nats_ptr->on_cmd([](auto&& cmd){});
	// 

	while (!bb_worker_thread_exit)
	{
		// working ...
		SleepEx(5000, TRUE);

		if (bb_worker_thread_run)
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

DWORD __stdcall _TimerThread(LPVOID)
{
	if (!hh_timer)
	{
		return 0;
	}

	// Alertable Thread;
	auto ret = WaitForSingleObjectEx(hh_timer, INFINITE, TRUE);
	if (ret == WAIT_ABANDONED)
	{

	}
	else if (ret == WAIT_IO_COMPLETION) // _TimerCallback
	{

	}
	else if (ret == WAIT_OBJECT_0) // SetWaitableTimerEx
	{

	}
	else if (ret == WAIT_TIMEOUT)
	{

	}
	else if (ret == WAIT_FAILED)
	{

	}
	else
	{

	}

	return 0;
}

void __stdcall _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{

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

	hh_waitable_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!hh_waitable_event)
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
	bb_worker_thread_run = true;
	OutputDebugString(L"@rg service starting ...\n");

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
			auto _msg = std::format(L"@rg WaitForSingleObject: WAIT_FAILED: {}\n"sv, err);
			OutputDebugString(_msg.c_str());
		}
		else
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
	bb_worker_thread_exit = true;
	bb_worker_thread_run = false;

	if (hh_worker_thread)
	{
		::CloseHandle(hh_worker_thread);
		hh_worker_thread = nullptr;
	}

	if (hh_timer_thread)
	{
		CloseHandle(hh_timer_thread);
		hh_timer_thread = nullptr;
	}

	if (hh_waitable_event)
	{
		::CloseHandle(hh_waitable_event);
		hh_waitable_event = nullptr;
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
		bb_worker_thread_run = false;
		OutputDebugString(L"@rg Recv [pause] control code: dosomething ...\n");
		update_status(ss_handle, SERVICE_PAUSED);
		OutputDebugString(L"@rg Recv [pause] control code: Service Paused.\n");
		break;
	case SERVICE_CONTROL_CONTINUE:
		// resume service
		OutputDebugString(L"@rg Recv [resume] control code.\n");
		update_status(ss_handle, SERVICE_CONTINUE_PENDING);
		bb_worker_thread_run = true;
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
		bb_worker_thread_exit = true;
		bb_worker_thread_run = false;
		SetEvent(hh_waitable_event);
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
	bb_worker_thread_exit = false;
	if (!hh_worker_thread)
	{
		hh_worker_thread = ::CreateThread(nullptr, 0, _WorkerThread, nullptr, 0, nullptr);
		if (!hh_worker_thread)
		{
			OutputDebugString(L"@rg service create worker thread failure.\n");
			bb_worker_thread_exit = true;
			bb_worker_thread_run = false;
			return false;
		}
	}

	if (!hh_timer_thread)
	{
		hh_timer_thread = ::CreateThread(nullptr, 0, _TimerThread, nullptr, 0, nullptr);

	}

	do
	{
		auto _name = L"Local\\WatcherTimer";
		hh_timer = ::CreateWaitableTimerEx(
			nullptr,
			_name,
			CREATE_WAITABLE_TIMER_MANUAL_RESET,
			TIMER_ALL_ACCESS);
		auto err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS)
		{
			CloseHandle(hh_timer);
			hh_timer = nullptr;
		}
		else if (err == ERROR_INVALID_HANDLE)
		{
			_name = nullptr;
		}
		else
		{
			auto _msg = std::format(L"@rg service create timer thread error: {}\n"sv, err);
			OutputDebugString(_msg.c_str());
			break;
		}

	} while (!hh_timer);

	if (!hh_timer)
	{
		OutputDebugString(L"@rg service create timer thread failure.\n");
		return false;
	}

	LARGE_INTEGER due_time; // 100nanosecond
	due_time.QuadPart = (LONGLONG)(-30 * TIMER_SECOND);
	LONG period = 30000; // millisecond
	REASON_CONTEXT reason{
		.Version = POWER_REQUEST_CONTEXT_VERSION,
		.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING,
	};
	wchar_t sr[] = L"";
	reason.Reason.SimpleReasonString = sr;
	ULONG delay = 5000;
	auto setted = ::SetWaitableTimerEx(hh_timer, &due_time, period, _TimerCallback, nullptr, &reason, delay);
	if (!setted)
	{
		auto err = GetLastError();
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

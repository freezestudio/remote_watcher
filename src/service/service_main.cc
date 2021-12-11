//
// exe service entry
//

#include "dep.h"
#include "service_dep.h"
#include "service_watch.h"
#include "service_nats_client.h"

#define SERVICE_TEST

// file time: 100 nanosecond
constexpr auto time_second = 10000000L;

//
// TODO: #include <netlistmgr.h>
// com: INetworkConnection::get_IsConnected
//

// service status handle.
SERVICE_STATUS_HANDLE ss_handle = nullptr;
HANDLE hh_waitable_event = nullptr;
DWORD cp_check_point = 0;

// work thread handle.
HANDLE hh_worker_thread = nullptr;
// work thread status.
bool bb_worker_thread_exit = false;
fs::path g_work_folder;

// timer thread handle.
HANDLE hh_timer_thread = nullptr;
bool bb_timer_thread_exit = false;

// sleep thread handle.
HANDLE hh_sleep_thread = nullptr;
// sleep thread state
bool bb_sleep_thread_exit = false;

// remote address.
std::wstring wcs_ip;
freeze::nats_client g_nats_client{};

void init_service();
bool init_threadpool();
bool update_status(SERVICE_STATUS_HANDLE, DWORD, DWORD = NO_ERROR);

DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID);
DWORD __stdcall _TimerThread(LPVOID);
DWORD __stdcall _SleepThread(LPVOID);

// timer thread callback
void __stdcall _TimerCallback(LPVOID, DWORD, DWORD);

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID)
{
	auto watcher = freeze::watchor{};
	while (!bb_worker_thread_exit)
	{
		// working ...
		SleepEx(INFINITE, TRUE);
		if (freeze::detail::check_exist(g_work_folder))
		{
			g_work_folder = freeze::detail::to_normal(g_work_folder);
		}
		if (watcher.set_folder(g_work_folder))
		{
			watcher.start();
		}
	}
	return 0;
}

DWORD __stdcall _TimerThread(LPVOID)
{

	HANDLE hh_timer = nullptr;
	auto _timer_name = L"Local\\Watcher_Timer";
	do
	{
		hh_timer = ::CreateWaitableTimerEx(
			nullptr,
			_timer_name,
			CREATE_WAITABLE_TIMER_MANUAL_RESET, // manual reset
			TIMER_ALL_ACCESS);
		if (!hh_timer)
		{
			auto err = GetLastError();
			if (err == ERROR_ALREADY_EXISTS)
			{
				if (hh_timer)
				{
					CloseHandle(hh_timer);
					hh_timer = nullptr;
				}
			}
			else if (err == ERROR_INVALID_HANDLE)
			{
				_timer_name = nullptr;
			}
			else
			{
				auto _msg = std::format(L"@rg service create timer thread error: {}\n"sv, err);
				OutputDebugString(_msg.c_str());
			}
		}
	} while (!hh_timer);

	if (!hh_timer)
	{
		OutputDebugString(L"@rg service create timer failure.\n");
		return 0;
	}

	LARGE_INTEGER due_time{}; // 100 nanosecond
	due_time.QuadPart = static_cast<ULONGLONG>(-30 * time_second);
	LONG period = 30000; // millisecond
	wchar_t reason_string[] = L"heart-beat";
	REASON_CONTEXT reason{
		POWER_REQUEST_CONTEXT_VERSION,
		POWER_REQUEST_CONTEXT_SIMPLE_STRING,
	};
	reason.Reason.SimpleReasonString = reason_string;
	ULONG delay = 5000; // tolerable delay
	auto ok = ::SetWaitableTimerEx(hh_timer, &due_time, period, _TimerCallback, (LPVOID)(&g_nats_client), &reason, delay);
	if (!ok)
	{
		auto err = GetLastError();
		auto _msg = std::format(L"@rg service set timer error: {}\n"sv, err);
		OutputDebugString(_msg.c_str());
		return 0;
	}

	while (!bb_timer_thread_exit)
	{
		// Alertable Thread;
		auto ret = SleepEx(INFINITE, TRUE);
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
	}

	if (hh_timer)
	{
		CancelWaitableTimer(hh_timer);
		CloseHandle(hh_timer);
		hh_timer = nullptr;
	}

	return 0;
}

void _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	auto ncp = reinterpret_cast<freeze::nats_client*>(lpArgToCompletionRoutine);
	if (!ncp)
	{
		OutputDebugString(L"@rg TimerCallback: args is null.\n");
		return;
	}

	auto _timout = LARGE_INTEGER{ dwTimerLowValue, (LONG)dwTimerHighValue }.QuadPart;
	auto _msg = std::format(L"@rg TimerCallback: ETA: {}\n."sv, _timout);
	OutputDebugString(_msg.c_str());
}

DWORD __stdcall _SleepThread(LPVOID)
{
	// run nats client
	// ...

	while (!bb_sleep_thread_exit)
	{
		SleepEx(INFINITE, TRUE);
	}
	return 0;
}

void init_service()
{
#ifndef SERVICE_TEST
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
#endif

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

#ifndef SERVICE_TEST
	// service running
	update_status(ss_handle, SERVICE_RUNNING);
	OutputDebugString(L"@rg service starting ...\n");
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
#ifndef SERVICE_TEST
	// stop service
	update_status(ss_handle, SERVICE_STOPPED);
#endif

	cp_check_point = 0;
	bb_worker_thread_exit = true;
	bb_timer_thread_exit = true;
	bb_sleep_thread_exit = true;

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
		OutputDebugString(L"@rg Recv [pause] control code: dosomething ...\n");
		update_status(ss_handle, SERVICE_PAUSED);
		OutputDebugString(L"@rg Recv [pause] control code: Service Paused.\n");
		break;
	case SERVICE_CONTROL_CONTINUE:
		// resume service
		OutputDebugString(L"@rg Recv [resume] control code.\n");
		update_status(ss_handle, SERVICE_CONTINUE_PENDING);
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
			return false;
		}
	}

	bb_timer_thread_exit = false;
	if (!hh_timer_thread)
	{
		hh_timer_thread = ::CreateThread(nullptr, 0, _TimerThread, nullptr, 0, nullptr);
		if (!hh_timer_thread)
		{
			bb_timer_thread_exit = true;
			OutputDebugString(L"@rg service create timer thread failure.\n");
			return false;
		}
	}

	bb_sleep_thread_exit = false;
	if (!hh_sleep_thread)
	{
		hh_sleep_thread = ::CreateThread(nullptr, 0, _SleepThread, nullptr, 0, nullptr);
		if (!hh_sleep_thread)
		{
			bb_sleep_thread_exit = true;
			OutputDebugString(L"@rg service create sleep thread failure.\n");
			return false;
		}
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
		wcs_ip = argv[1];
		auto _msg = std::format(L"@rg ServiceMain: argc={}, RemoteIP is: {}\n"sv, argc, wcs_ip);
		OutputDebugString(_msg.data());
	}
	for (auto i = 0; i < argc; ++i)
	{
		auto _msg = std::format(L"@rg ServiceMain: argc[{}], {}\n"sv, i, argv[i]);
		OutputDebugString(_msg.c_str());
	}

	if (wcs_ip.empty())
	{
		OutputDebugString(L"@rg ServiceMain: ip is null.\n");
	}
	else
	{
		if (freeze::detail::make_ip_address(wcs_ip) == 0)
		{
			OutputDebugString(L"@rg ServiceMain: ip is wrong.\n");
		}
		else
		{
			OutputDebugString(L"@rg ServiceMain: rgmsvc starting ...\n");
			init_service();
		}
	}

	OutputDebugString(L"@rg ServiceMain: rgmsvc stopped.\n");
}

int __stdcall wmain()
{
#ifdef SERVICE_TEST
	wchar_t arg1[] = L"rgmsvc";
	wchar_t arg2[] = L"192.168.2.95";
	wchar_t* argv[] = {
		arg1,
		arg2,
	};
	ServiceMain(2, argv);
#else
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
#endif
	OutputDebugString(L"@rg main: Done.\n");
	}

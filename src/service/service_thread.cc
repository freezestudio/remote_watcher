//
// parallel threads
//
#include "service.h"
#include "service_extern.h"

// file time: 100 nanosecond.
constexpr auto time_second = 10000000L;

// work thread handle.
HANDLE hh_worker_thread = nullptr;
// work thread status.
bool bb_worker_thread_exit = false;

// timer thread handle.
HANDLE hh_timer_thread = nullptr;
bool bb_timer_thread_exit = false;

// sleep thread handle.
HANDLE hh_sleep_thread = nullptr;
// sleep thread state.
bool bb_sleep_thread_exit = false;

/* extern maybe noused */
std::vector<fs::path> g_work_ignore_folders;
// connect to remote address.
static freeze::nats_client g_nats_client{};
// global signal for communicate-with-nats with reason.
freeze::atomic_sync_reason global_reason_signal{};

/*extern*/
fs::path g_work_folder;
void reset_work_folder(bool notify/* = false */)
{
	auto wcs_folder = freeze::detail::read_latest_folder();
	DEBUG_STRING(L"@rg reset_work_folder(): {}, need notify={}.\n"sv, wcs_folder, notify);
	if (wcs_folder.empty())
	{
		DEBUG_STRING(L"@rg reset_work_folder(): folder is null.\n");
		return;
	}

	auto mbs_folder = freeze::detail::to_utf8(wcs_folder);
	auto latest_folder = freeze::detail::to_utf16(mbs_folder);
	auto latest_path = fs::path{ latest_folder };
	if (!fs::exists(latest_path))
	{
		DEBUG_STRING(L"@rg reset_work_folder(): {}, folder not exists.\n"sv, latest_path.c_str());
		return;
	}

	if (g_work_folder == latest_path)
	{
		DEBUG_STRING(L"@rg reset_work_folder(): {}, already watched.\n"sv, latest_path.c_str());
		return;
	}

	// TODO: add write lock (want using atomic)
	g_work_folder = latest_path;
	if (notify)
	{
		DEBUG_STRING(L"@rg reset_work_folder(): Want Wakeup work-thread ...\n");
		auto ret = QueueUserAPC([](ULONG_PTR) {}, hh_worker_thread, 0);
		if (!ret)
		{
			auto err = GetLastError();
			DEBUG_STRING(L"@rg reset_work_folder(): Wakup work-thread error: {}\n"sv, err);
		}
		else
		{
			DEBUG_STRING(L"@rg reset_work_folder(): work-thread will Wakup.\n");
		}
	}
}

// worker thread.
DWORD __stdcall _WorkerThread(LPVOID)
{
	DEBUG_STRING(L"@rg WorkerThread: Starting ...\n");
	auto underline_watch = freeze::folder_watchor_apc{};
	// auto underline_watch = freeze::folder_watchor_status{};
	// auto underline_watch = freeze::folder_watchor_result{};
	auto watcher = freeze::watcher_win{ underline_watch };

	if (freeze::detail::check_exists(g_work_folder))
	{
		watcher.set_watch_folder(g_work_folder);
		watcher.set_ignore_folders(g_work_ignore_folders);
		watcher.start();
		DEBUG_STRING(L"@rg WorkerThread: Current folder [{}], Watcher started.\n"sv, g_work_folder.c_str());
	}
	else
	{
		DEBUG_STRING(L"@rg WorkerThread: Current folder [{}], not exists, watcher not started.\n"sv, g_work_folder.c_str());
	}

	while (!bb_worker_thread_exit)
	{
		DEBUG_STRING(L"@rg WorkerThread: Sleep, waiting wakeup ...\n");
		// want wakeup from modify-folder command.
		SleepEx(INFINITE, TRUE);
		DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup.\n");

#ifndef SERVICE_TEST
		// maybe service paused.
		if (get_service_status() != freeze::service_state::running)
		{
			DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup, but Service not running.\n");
			continue;
		}
#endif

		if (freeze::detail::check_exists(g_work_folder))
		{
			watcher.stop();
			watcher.set_watch_folder(g_work_folder);
			watcher.set_ignore_folders(g_work_ignore_folders);
			watcher.start();
		}
		else
		{
			DEBUG_STRING(L"@rg WorkerThread: when wakeup, the folder: {}, maybe is null.\n"sv, g_work_folder.c_str());
		}
	}

	DEBUG_STRING(L"@rg WorkerThread: try stop watcher ...\n");
	watcher.stop();
	DEBUG_STRING(L"@rg WorkerThread: Stopped.\n");
	return 0;
}

DWORD __stdcall _TimerThread(LPVOID)
{
	DEBUG_STRING(L"@rg TimerThread: Starting ...\n");
	HANDLE hh_timer = nullptr;
	auto _timer_name = L"Local\\Watcher_Timer";
	do
	{
		hh_timer = ::CreateWaitableTimerEx(
			nullptr,                            // LPSECURITY_ATTRIBUTES
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
				DEBUG_STRING(L"@rg TimerThread: CreateWaitableTimerEx error: {}\n"sv, err);
			}
		}
	} while (!hh_timer);

	if (!hh_timer)
	{
		DEBUG_STRING(L"@rg TimerThread: CreateWaitableTimerEx failure.\n");
		return 0;
	}

	LARGE_INTEGER due_time{}; // 100 nanosecond
	due_time.QuadPart = static_cast<ULONGLONG>(-30 * time_second);
	LONG period = 30 * 1000; // milli-second
	wchar_t reason_string[] = L"heart-beat";
	REASON_CONTEXT reason{
		POWER_REQUEST_CONTEXT_VERSION,
		POWER_REQUEST_CONTEXT_SIMPLE_STRING,
	};
	reason.Reason.SimpleReasonString = reason_string;
	ULONG delay = 0; // tolerable delay 5000 milli-second
	auto ok = ::SetWaitableTimerEx(
		hh_timer,
		&due_time,
		period,
		_TimerCallback,
		(LPVOID)(&g_nats_client), // lpArgToCompletionRoutine
		&reason,
		delay
	);
	if (!ok)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg TimerThread: set timer error: {}, stop.\n"sv, err);
		return 0;
	}
	DEBUG_STRING(L"@rg TimerThread: SetWaitableTimerEx Successfully.\n");

	while (!bb_timer_thread_exit)
	{
		DEBUG_STRING(L"@rg TimerThread: Block Waiting ...\n");
		// Alertable Thread;
		SleepEx(INFINITE, TRUE);
	}

	if (hh_timer)
	{
		CancelWaitableTimer(hh_timer);
		CloseHandle(hh_timer);
		hh_timer = nullptr;
	}
	DEBUG_STRING(L"@rg TimerThread: Stopped.\n");
	return 0;
}

DWORD __stdcall _SleepThread(LPVOID)
{
	DEBUG_STRING(L"@rg SleepThread: Starting ...\n");
	// TODO: check nats can connection ... (in timer-thread)

	auto ip = reset_ip_address();
	if (ip > 0)
	{
		DEBUG_STRING(L"@rg SleepThread: reset remote ip {}.\n"sv, ip);
	}
	else
	{
		DEBUG_STRING(L"@rg SleepThread Error: remote ip is null, {}, stop.\n"sv, reset_ip_error(ip));
		// notify other thread and service stop.
		return 0;
	}

	auto connected = g_nats_client.connect(ip);
	if (connected)
	{
		DEBUG_STRING(L"@rg SleepThread: remote connected.\n");
	}
	else
	{
		DEBUG_STRING(L"@rg SleepThread Error: remote not connected.\n");
		return 0;
	}

	while (!bb_sleep_thread_exit)
	{
		DEBUG_STRING(L"@rg SleepThread: Waiting Wakeup ...\n");
		//wait until spec reason changed.
		auto reason = global_reason_signal.wait_reason();
		DEBUG_STRING(L"@rg SleepThread: wakeup reason: {}.\n"sv, reason_string(reason));
#ifndef SERVICE_TEST
		if (!is_service_running())
		{
			DEBUG_STRING(L"@rg SleepThread: wakeup, maybe service paused.\n");
			continue;
		}
#endif

		if (!g_nats_client.is_connected())
		{
			auto _ip = reset_ip_address();
			if (_ip <= 0)
			{
				DEBUG_STRING(L"@rg SleepThread: wakeup error remote ip is null, {}, stop.\n"sv, reset_ip_error(_ip));
				continue;
			}

			auto connected = g_nats_client.connect(_ip);
			if (!connected)
			{
				DEBUG_STRING(L"@rg SleepThread wakeup error: remote not connected, stop.\n");
				continue;
			}
		}

#ifndef SERVICE_TEST
		// this want pause timer-thread (SERVICE_PAUSED=7)
		set_service_status(SERVICE_PAUSED);
		DEBUG_STRING(L"@rg SleepThread: wakeup Pause-TimerThread.\n");
#endif

		switch (reason)
		{
		default: [[fallthrough]];
		case sync_reason_none__reason:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_none__reason(0).\n");
			break;
		case sync_reason_recv_command:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_recv_command(1).\n");
			freeze::maybe_response_command(g_nats_client);
			break;
		case sync_reason_recv_message:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_recv_message(2).\n");
			freeze::maybe_send_message(g_nats_client);
			break;
		case sync_reason_send_command:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_send_command(3).\n");
			break;
		case sync_reason_send_message:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_send_message(4).\n");
			break;
		case sync_reason_send_payload:
			DEBUG_STRING(L"@rg SleepThread: wakup: sync_reason_send_payload(5).\n");
			// if reason is folder changed event emitted.
			freeze::maybe_send_payload(g_nats_client, g_work_folder);
			break;
		}

#ifndef SERVICE_TEST
		// resume timer-thread (SERVICE_RUNNING=4)
		set_service_status(SERVICE_RUNNING);
		DEBUG_STRING(L"@rg SleepThread: wakeup Resume-TimerThread.\n");
#endif
	}

	DEBUG_STRING(L"@rg SleepThread: Stopped.\n");
	g_nats_client.close();
	return 0;
}

bool init_threadpool()
{
	bb_worker_thread_exit = false;
	if (!hh_worker_thread)
	{
		hh_worker_thread = ::CreateThread(nullptr, 0, _WorkerThread, nullptr, 0, nullptr);
		if (!hh_worker_thread)
		{
			DEBUG_STRING(L"@rg init_threadpool(): Create WorkerThread failure.\n");
			bb_worker_thread_exit = true;
			return false;
		}
		if (hh_worker_thread == INVALID_HANDLE_VALUE)
		{
			DEBUG_STRING(L"@rg init_threadpool(): Create WorkerThread Error: INVALID_HANDLE_VALUE.\n");
			hh_worker_thread = nullptr;
			bb_worker_thread_exit = true;
			return false;
		}
	}
	DEBUG_STRING(L"@rg init_threadpool(): Create WorkerThread Successfully.\n");

	bb_timer_thread_exit = false;
	if (!hh_timer_thread)
	{
		hh_timer_thread = ::CreateThread(nullptr, 0, _TimerThread, nullptr, 0, nullptr);
		if (!hh_timer_thread)
		{
			bb_timer_thread_exit = true;
			DEBUG_STRING(L"@rg init_threadpool(): Create TimerThread Failure.\n");
			return false;
		}
		if (hh_timer_thread == INVALID_HANDLE_VALUE)
		{
			DEBUG_STRING(L"@rg init_threadpool(): Create TimerThread Error: INVALID_HANDLE_VALUE.\n");
			hh_timer_thread = nullptr;
			bb_timer_thread_exit = true;
			return false;
		}
	}
	DEBUG_STRING(L"@rg init_threadpool(): Create TimerThread Successfully.\n");

	bb_sleep_thread_exit = false;
	if (!hh_sleep_thread)
	{
		hh_sleep_thread = ::CreateThread(nullptr, 0, _SleepThread, nullptr, 0, nullptr);
		if (!hh_sleep_thread)
		{
			bb_sleep_thread_exit = true;
			DEBUG_STRING(L"@rg init_threadpool(): Create SleepThread Failure.\n");
			return false;
		}
		if (hh_sleep_thread == INVALID_HANDLE_VALUE)
		{
			DEBUG_STRING(L"@rg init_threadpool(): Create SleepThread Error: INVALID_HANDLE_VALUE.\n");
			hh_sleep_thread = nullptr;
			bb_sleep_thread_exit = true;
			return false;
		}
	}
	DEBUG_STRING(L"@rg init_threadpool(): Create SleepThread Successfully.\n");

	auto success = hh_worker_thread && hh_timer_thread && hh_sleep_thread;
	auto running = !bb_worker_thread_exit && !bb_timer_thread_exit && !bb_sleep_thread_exit;
	return success && running;
}

void stop_threadpool()
{
	bb_worker_thread_exit = true;
	auto ret = QueueUserAPC([](ULONG_PTR) {}, hh_worker_thread, 0);
	if (ret == 0)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg stop_threadpool(): notify WorkThread stop failure: {}.\n", err);
	}
	else
	{
		DEBUG_STRING(L"@rg stop_threadpool(): notify WorkThread stop.\n");
	}

	bb_timer_thread_exit = true;
	ret = QueueUserAPC([](ULONG_PTR) {}, hh_timer_thread, 0);
	if (ret == 0)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg stop_threadpool(): notify TimerThread stop failure: {}.\n", err);
	}
	else
	{
		DEBUG_STRING(L"@rg stop_threadpool(): notify TimerThread stop.\n");
	}

	bb_sleep_thread_exit = true;
	global_reason_signal.notify_reason(sync_reason_exit__thread);
	DEBUG_STRING(L"@rg stop_threadpool(): notify SleepThread stop.\n");

	Sleep(3000);
	DEBUG_STRING(L"@rg stop_threadpool(): wait 3s done.\n");

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

	if (hh_sleep_thread)
	{
		CloseHandle(hh_sleep_thread);
		hh_sleep_thread = nullptr;
	}

	DEBUG_STRING(L"@rg stop_threadpool(): Threadpool Stopped.\n");
}

namespace freeze
{
	rgm_service::rgm_service()
		: _service_status_handle{ nullptr }
		, _service_status{}
		, _check_point{ 0 }
		, _signal{}
	{
		_service_status.dwServiceType = SERVICE_WIN32;
		_service_status.dwServiceSpecificExitCode = 0;

		_rgm_worker = service_object_instance<rgm_worker>();
		_rgm_timer = service_object_instance<rgm_timer>();
		_rgm_nats = service_object_instance<rgm_nats>();
	}

	rgm_service::~rgm_service()
	{

	}

	bool rgm_service::initialize()
	{
		_service_status_handle = ::RegisterServiceCtrlHandlerEx(
			SERVICE_NAME,
			rgm_service::control_code_handle_ex,
			(LPVOID)(this)
		);
		if (!_service_status_handle)
		{
			return false;
		}

		return update_status(service_state::start_pending);
	}

	void rgm_service::start()
	{
		_rgm_worker->start();
		_rgm_timer->start();
		_rgm_nats->start();

		update_status(service_state::running);

		// main thread waiting until stopped ...
		_signal.wait();
	}

	void rgm_service::stop()
	{
		_signal.notify();
		update_status(service_state::stopped);

		_rgm_worker->stop();
		_rgm_timer->stop();
		_rgm_nats->stop();
	}

	void rgm_service::pause()
	{
		_rgm_worker->pause();
		_rgm_timer->pause();
		_rgm_nats->pause();

		update_status(service_state::paused);
	}

	void rgm_service::resume()
	{
		_rgm_worker->resume();
		_rgm_timer->resume();
		_rgm_nats->resume();

		update_status(service_state::running);
	}

	bool rgm_service::update_status(service_state state, DWORD error_code)
	{
		if (!_service_status_handle)
		{
			return false;
		}

		_service_status.dwCurrentState = to_dword(state);
		_service_status.dwWin32ExitCode = error_code;

		if (state == service_state::start_pending or state == service_state::stopped)
		{
			_service_status.dwControlsAccepted = 0;
		}
		else
		{
			_service_status.dwControlsAccepted = service_accept::param_change |
				service_accept::pause_continue |
				service_accept::shutdown |
				service_accept::stop;
		}

		if (state == service_state::running or state == service_state::stopped)
		{
			_service_status.dwCheckPoint = 0;
			_service_status.dwWaitHint = 0;
		}
		else
		{
			_service_status.dwCheckPoint = _check_point++;
			_service_status.dwWaitHint = 3000;
		}

		return ::SetServiceStatus(
			_service_status_handle, &_service_status) ? true : false;
	}

	DWORD __stdcall rgm_service::control_code_handle_ex(
		DWORD code, DWORD event_type, LPVOID lpevdata, LPVOID lpcontext)
	{
		auto self = reinterpret_cast<rgm_service*>(lpcontext);
		if (!self)
		{
			return ERROR_INVALID_HANDLE;
		}

		service_control control_code = static_cast<service_control>(code);
		switch (control_code)
		{
		case freeze::service_control::shutdown:
			[[fallthrough]];
		case freeze::service_control::stop:
			self->update_status(service_state::stop_pending);
			self->stop();
			break;
		case freeze::service_control::network_disconnect:
			[[fallthrough]];
		case freeze::service_control::pause:
			self->update_status(service_state::pause_pending);
			self->pause();
			break;
		case freeze::service_control::network_connect:
			[[fallthrough]];
		case freeze::service_control::_continue:
			self->update_status(service_state::continue_pending);
			self->resume();
			break;
		case freeze::service_control::param_change:
			break;
		default:
			break;
		}
		return NO_ERROR;
	}
}

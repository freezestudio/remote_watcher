//
// parallel threads
//
#include "service.h"
#include "service_extern.h"

// file time: 100 nanosecond.
constexpr auto time_second = 10000000L;
constexpr auto timer_period = 2 * 60 * 1000L;

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
/* extern */
freeze::atomic_sync_reason global_reason_signal{};
/* extern */
long global_reason_worker{0L};
/*extern*/
fs::path g_work_folder;

void reset_work_folder(bool notify /* = false */)
{
	// TODO: maybe need lock
	auto wcs_folder = freeze::detail::read_latest_folder();
	DEBUG_STRING(L"@rg reset_work_folder(): read latest={}, notify-to-WorkThread={}.\n"sv, wcs_folder, notify);

	if (wcs_folder.empty())
	{
		DEBUG_STRING(L"@rg reset_work_folder(): folder is null.\n");
		return;
	}

	auto mbs_folder = freeze::detail::to_utf8(wcs_folder);
	auto latest_folder = freeze::detail::to_utf16(mbs_folder);
	auto latest_path = fs::path{latest_folder};
	if (!fs::exists(latest_path))
	{
		DEBUG_STRING(L"@rg reset_work_folder(): error, folder={}, not exists.\n"sv, latest_path.c_str());
		return;
	}

	if (g_work_folder == latest_path)
	{
		DEBUG_STRING(L"@rg reset_work_folder(): folder={}, already watched, ignore.\n"sv, latest_path.c_str());
		return;
	}

	// TODO: add write lock (want using atomic)
	g_work_folder = latest_path;
	DEBUG_STRING(L"@rg reset_work_folder(): set g_work_folder={}.\n"sv, g_work_folder.c_str());

	if (notify)
	{
		DEBUG_STRING(L"@rg reset_work_folder(): Will Wakeup _WorkThread ...\n");
		auto ret = QueueUserAPC([](ULONG_PTR) {}, hh_worker_thread, 0);
		if (ret)
		{
			DEBUG_STRING(L"@rg reset_work_folder(): Wakeup _WorkThread done.\n");
		}
		else
		{
			auto err = GetLastError();
			DEBUG_STRING(L"@rg reset_work_folder(): Wakeup _WorkThread error: {}\n"sv, err);
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
	auto watcher = freeze::watcher_win32{underline_watch};

	if (freeze::detail::check_exists(g_work_folder))
	{
		watcher.set_watch_folder(g_work_folder);
		watcher.set_ignore_folders(g_work_ignore_folders);
		watcher.start();
		DEBUG_STRING(L"@rg WorkerThread: watch-folder={}, Watcher started.\n"sv, g_work_folder.c_str());
	}
	else
	{
		DEBUG_STRING(L"@rg WorkerThread: watch-folder={}, not exists, watcher not started.\n"sv, g_work_folder.c_str());
	}

	// thread sleep ...
	while (!bb_worker_thread_exit)
	{
		DEBUG_STRING(L"@rg WorkerThread: Sleep, Waiting Wakeup ...\n");
		// want wakeup from modify-folder command.
		SleepEx(INFINITE, TRUE);

		DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup, running...\n");

#ifndef SERVICE_TEST
		// maybe service paused.
		if (get_service_status() != freeze::service_state::running)
		{
			DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup, but [rgmsvc] Service not running.\n");
			continue;
		}
#endif
		if (global_reason_worker == work_reason_act__empty)
		{
			DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup, maybe heartbeat reason.\n");
			continue;
		}

		// task: change watch folder
		if (freeze::detail::check_exists(g_work_folder))
		{
			watcher.stop();
			watcher.set_watch_folder(g_work_folder);
			watcher.set_ignore_folders(g_work_ignore_folders);
			watcher.start();
			DEBUG_STRING(L"@rg WorkerThread: when Wakeup, set watch-folder={}, Watcher re-started.\n"sv, g_work_folder.c_str());
		}
		else
		{
			DEBUG_STRING(L"@rg WorkerThread: when Wakeup, the watch-folder={}, maybe is null.\n"sv, g_work_folder.c_str());
		}
		DEBUG_STRING(L"@rg WorkerThread: Alerable Wakeup, Done.\n");
	}

	DEBUG_STRING(L"@rg WorkerThread: try stop watcher ...\n");
	watcher.stop();
	DEBUG_STRING(L"@rg WorkerThread: Stopped.\n");
	return 0;
}

// timer thread.
DWORD __stdcall _TimerThread(LPVOID)
{
	DEBUG_STRING(L"@rg TimerThread: Starting ...\n");
	HANDLE hh_timer = nullptr;
	auto _timer_name = L"Local\\Watcher_Timer";
	do
	{
		hh_timer = ::CreateWaitableTimerEx(
			nullptr, // LPSECURITY_ATTRIBUTES
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
	LONG period = timer_period; // milli-second
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
		delay);
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

// sleep thread.
DWORD __stdcall _SleepThread(LPVOID)
{
	DEBUG_STRING(L"@rg SleepThread: Starting ...\n");
	// TODO: check nats can connection ... (in timer-thread)

	auto ip = reset_ip_address();
	if (ip < 0)
	{
		DEBUG_STRING(L"@rg SleepThread Error: remote ip is null, {}, stop.\n"sv, reset_ip_error(ip));
		// notify other thread and service stop.
		return 0;
	}
	else
	{
		DEBUG_STRING(L"@rg SleepThread: Reset to RemoteIp: {}.\n"sv, ip);
	}

	auto connected = g_nats_client.connect(ip);
	if (connected)
	{
		DEBUG_STRING(L"@rg SleepThread: remote connected.\n");
	}
	else
	{
		DEBUG_STRING(L"@rg SleepThread Error: remote not connected, stop.\n");
		return 0;
	}

	// thread sleep ...
	while (!bb_sleep_thread_exit)
	{
		DEBUG_STRING(L"@rg SleepThread: Waiting Wakeup ...\n");

		// wait until some reason changed.
		auto reason = global_reason_signal.wait_reason();
		DEBUG_STRING(L"@rg SleepThread: Wakeup Reason: {}.\n"sv, reason_string(reason));
		if (reason == sync_reason_exit__thread)
		{
			break;
		}

#ifndef SERVICE_TEST
		if (!is_service_running())
		{
			DEBUG_STRING(L"@rg SleepThread: Wakeup, but service maybe paused, continue.\n");
			continue;
		}
#endif

		if (!g_nats_client.is_connected())
		{
			auto _ip = reset_ip_address();
			if (_ip < 0)
			{
				DEBUG_STRING(L"@rg SleepThread: Wakeup error remote ip is null, {}, continue.\n"sv, reset_ip_error(_ip));
				continue;
			}

			auto connected = g_nats_client.connect(_ip);
			if (!connected)
			{
				DEBUG_STRING(L"@rg SleepThread Wakeup error: remote not connected, continue.\n");
				continue;
			}
		}

#ifndef SERVICE_TEST
		// this want pause timer-thread (SERVICE_PAUSED=7)
		set_service_status(SERVICE_PAUSED);
		DEBUG_STRING(L"@rg SleepThread: Wakeup Pause-TimerThread.\n");
#endif

		// TODO: timeout if blocking
		switch (reason)
		{
		default:
			[[fallthrough]];
		case sync_reason_none__reason:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_none__reason(0).\n");
			break;
		case sync_reason_recv_command:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_recv_command(1), waiting...\n");
			freeze::maybe_response_command(g_nats_client);
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_recv_command(1) done.\n");
			break;
		case sync_reason_recv_message:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_recv_message(2).\n");
			freeze::maybe_send_message(g_nats_client);
			break;
		case sync_reason_send_command:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_send_command(3).\n");
			break;
		case sync_reason_send_message:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_send_message(4).\n");
			break;
		case sync_reason_send_payload:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_send_payload(5).\n");
			// if reason is folder changed event emitted.
			freeze::maybe_send_payload(g_nats_client, g_work_folder);
			break;
		case sync_reason_send_synfile:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_send_synfile(6).\n");
			freeze::maybe_send_synfile(g_nats_client);
			break;
		// response command
		case sync_reason_cmd__error:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_cmd__error(-2).\n");
			break;
		case sync_reason_cmd__empty:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_cmd__empty(10).\n");
			break;
		case sync_reason_cmd_folder:
			DEBUG_STRING(L"@rg SleepThread: Wakeup: sync_reason_cmd_folder(11).\n");
			reset_work_folder(true);
			break;
		}

#ifndef SERVICE_TEST
		// resume timer-thread (SERVICE_RUNNING=4)
		set_service_status(SERVICE_RUNNING);
		DEBUG_STRING(L"@rg SleepThread: Wakeup Resume-TimerThread.\n");
#endif
	}

	DEBUG_STRING(L"@rg SleepThread: Stopped.\n");
	g_nats_client.close();
	return 0;
}

static bool _create_worker_thread(bool can_create = false)
{
	bb_worker_thread_exit = false;
	if (!hh_worker_thread && can_create)
	{
		hh_worker_thread = ::CreateThread(nullptr, 0, _WorkerThread, nullptr, 0, nullptr);
		if (!hh_worker_thread)
		{
			DEBUG_STRING(L"@rg Create WorkerThread failure.\n");
			bb_worker_thread_exit = true;
			return false;
		}
		if (hh_worker_thread == INVALID_HANDLE_VALUE)
		{
			DEBUG_STRING(L"@rg Create WorkerThread Error: INVALID_HANDLE_VALUE.\n");
			hh_worker_thread = nullptr;
			bb_worker_thread_exit = true;
			return false;
		}
	}

	return !!hh_worker_thread;
}

static bool _create_sleep_thread(bool can_create = false)
{
	bb_sleep_thread_exit = false;
	if (!hh_sleep_thread && can_create)
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
	return !!hh_sleep_thread;
}

static bool _create_timer_thread()
{
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
	return !!hh_timer_thread;
}

bool init_threadpool(bool can_create /* = false */)
{
	if (!_create_timer_thread())
	{
		DEBUG_STRING(L"@rg init_threadpool(): Create TimerThread Failure, stop.\n");
	}
	DEBUG_STRING(L"@rg init_threadpool(): Create TimerThread Successfully.\n");

	_create_sleep_thread(can_create);
	DEBUG_STRING(L"@rg init_threadpool(): Create SleepThread Successfully.\n");

	_create_worker_thread(can_create);
	DEBUG_STRING(L"@rg init_threadpool(): Create WorkerThread Successfully.\n");

	bool success = hh_worker_thread && hh_timer_thread && hh_sleep_thread;
	auto running = !bb_worker_thread_exit && !bb_timer_thread_exit && !bb_sleep_thread_exit;
	auto all_true = success && running;
	if (!all_true)
	{
		DEBUG_STRING(L"@rg init_threadpool(): Not All Successed.\n");
	}

	return hh_timer_thread && !bb_timer_thread_exit;
}

void stop_threadpool()
{
	DWORD ret = 0;

	// stop sleep-thread
	bb_sleep_thread_exit = true;
	ret = QueueUserAPC([](ULONG_PTR)
					   { global_reason_signal.notify_reason(sync_reason_exit__thread); },
					   hh_timer_thread, 0);
	if (ret == 0)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg stop_threadpool(): notify SleepThread stop failure: {}.\n", err);
		TerminateThread(hh_sleep_thread, 0);
	}
	else
	{
		DEBUG_STRING(L"@rg stop_threadpool(): notify SleepThread stop.\n");
	}

	// stop timer-thread
	bb_timer_thread_exit = true;
	ret = QueueUserAPC([](ULONG_PTR) {}, hh_timer_thread, 0);
	if (ret == 0)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg stop_threadpool(): notify TimerThread stop failure: {}.\n", err);
		TerminateThread(hh_timer_thread, 0);
	}
	else
	{
		DEBUG_STRING(L"@rg stop_threadpool(): notify TimerThread stop.\n");
	}

	// stop worker-thread
	bb_worker_thread_exit = true;
	ret = QueueUserAPC([](ULONG_PTR) {}, hh_worker_thread, 0);
	if (ret == 0)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg stop_threadpool(): notify WorkThread stop failure: {}.\n", err);
		TerminateThread(hh_worker_thread, 0);
	}
	else
	{
		DEBUG_STRING(L"@rg stop_threadpool(): notify WorkThread stop.\n");
	}

	// Sleep(3000);
	// DEBUG_STRING(L"@rg stop_threadpool(): wait 3s done.\n");

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

bool test_sleep_thread()
{
	if (!hh_sleep_thread)
	{
		return _create_sleep_thread(true);
	}

	auto ret = QueueUserAPC([](ULONG_PTR)
							{ global_reason_signal.notify_reason(sync_reason_cmd__empty); },
							hh_timer_thread, 0);
	if (!ret)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg test_sleep_thread(): notify SleepThread stop failure: {}.\n", err);
		TerminateThread(hh_sleep_thread, 0);
		hh_sleep_thread = nullptr;
		return _create_sleep_thread(true);
	}
	else
	{
		return true;
	}
}

bool test_worker_thread()
{
	if (!hh_worker_thread)
	{
		return _create_worker_thread(true);
	}
	auto ret = QueueUserAPC([](ULONG_PTR)
							{ global_reason_worker=work_reason_act__empty; },
							hh_timer_thread, 0);
	if (!ret)
	{
		auto err = GetLastError();
		DEBUG_STRING(L"@rg bool test_worker_thread(): notify WorkerThread failure: {}.\n", err);
		TerminateThread(hh_worker_thread, 0);
		hh_worker_thread = nullptr;
		return _create_worker_thread(true);
	}
	else
	{
		return true;
	}
}

namespace freeze
{
	rgm_service::rgm_service()
		: _service_status_handle{nullptr}, _service_status{}, _check_point{0}, _signal{}
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
			(LPVOID)(this));
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
				   _service_status_handle, &_service_status)
				   ? true
				   : false;
	}

	DWORD __stdcall rgm_service::control_code_handle_ex(
		DWORD code, DWORD event_type, LPVOID lpevdata, LPVOID lpcontext)
	{
		auto self = reinterpret_cast<rgm_service *>(lpcontext);
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

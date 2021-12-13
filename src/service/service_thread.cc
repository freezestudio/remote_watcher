#include "service.h"


// file time: 100 nanosecond
constexpr auto time_second = 10000000L;

// work thread handle.
HANDLE hh_worker_thread = nullptr;
// work thread status.
bool bb_worker_thread_exit = false;
fs::path g_work_folder;
// global signal for folder-change-to-nats-thread
freeze::atomic_sync global_folder_change_signal{};

// timer thread handle.
HANDLE hh_timer_thread = nullptr;
bool bb_timer_thread_exit = false;

// sleep thread handle.
HANDLE hh_sleep_thread = nullptr;
// sleep thread state
bool bb_sleep_thread_exit = false;

// remote address.
freeze::nats_client g_nats_client{};

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID)
{
	auto watcher = freeze::watchor{};
#ifdef SERVICE_TEST
	if (watcher.set_folder(freeze::detail::to_normal(g_work_folder)))
	{
		watcher.start();
	}
#endif
	while (!bb_worker_thread_exit)
	{
		SleepEx(INFINITE, TRUE);
#ifndef SERVICE_TEST
		if (ss_current_status != freeze::service_state::running)
		{
			continue;
		}
#endif
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
			OutputDebugString(L"@rg timer thread waitable handle signed: WAIT_ABANDONED\n");
		}
		else if (ret == WAIT_IO_COMPLETION) // _TimerCallback
		{
			OutputDebugString(L"@rg timer thread waitable handle signed: WAIT_IO_COMPLETION\n");
		}
		else if (ret == WAIT_OBJECT_0) // SetWaitableTimerEx
		{
			OutputDebugString(L"@rg timer thread waitable handle signed: WAIT_OBJECT_0\n");
		}
		else if (ret == WAIT_TIMEOUT)
		{
			OutputDebugString(L"@rg timer thread waitable handle signed: WAIT_TIMEOUT\n");
		}
		else if (ret == WAIT_FAILED)
		{
			OutputDebugString(L"@rg timer thread waitable handle signed: WAIT_FAILED\n");
		}
		else
		{
			auto err = ::GetLastError();
			auto _msg = std::format(L"@rg SleepEx: timer thread waitable handle occuse an error: {}\n"sv, err);
			OutputDebugString(_msg.c_str());
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
	auto _msg = std::format(L"@rg TimerCallback: ETA: {}\n."sv, dwTimerLowValue);
	OutputDebugString(_msg.c_str());
}

DWORD __stdcall _SleepThread(LPVOID)
{
	freeze::nats_client nats_client;
	if (!wcs_ip.empty())
	{
		auto ip = freeze::detail::make_ip_address(wcs_ip);
		if (ip > 0)
		{
			nats_client.change_ip(ip);
		}
		else
		{
			OutputDebugString(L"@rg SleepThread: error invalid ip.\n");
			// notify other thread and service stop.
			return 0;
		}
	}
	else
	{
		OutputDebugString(L"@rg SleepThread: error remote ip is null.\n");
		// notify other thread and service stop.
		return 0;
	}

	while (!bb_sleep_thread_exit)
	{
		//wait folder changed.
		global_folder_change_signal.wait();
		auto& vec_changes = freeze::detail::get_changed_information();
		for (auto& d : vec_changes)
		{
			auto _msg = std::format(L"action={}, name={}\n"sv, d.action, d.filename);
			OutputDebugString(_msg.c_str());
		}
	}
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

void stop_threadpool()
{
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

	if (hh_sleep_thread)
	{
		CloseHandle(hh_sleep_thread);
		hh_sleep_thread = nullptr;
	}
}

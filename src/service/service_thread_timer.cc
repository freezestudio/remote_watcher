//
// heartbeat timer
//

#include "service_thread_timer.h"
#include "service_nats_client.h"
#include "service_extern.h"
#include "service_process.h"

DWORD _g_latest_time = 0;
static std::wstring heartbeat(freeze::nats_client * nc_ptr, DWORD dwTimerLowValue)
{
	// auto _timout = LARGE_INTEGER{ dwTimerLowValue, (LONG)dwTimerHighValue }.QuadPart;
	auto _timeout = static_cast<DWORD>((dwTimerLowValue - _g_latest_time) * 1e-7);
	_g_latest_time = dwTimerLowValue;
	DEBUG_STRING(L"@rg TimerCallback: ping duration: {}\n"sv, _timeout);

	// check other threads
	// 1. SleepThread
	// global_reason_signal.notify_reason(sync_reason_cmd__empty);
	test_sleep_thread();
	// 2. WorkerThread
	test_worker_thread();
	
	auto status = nc_ptr->maybe_heartbeat();
	auto status_msg = L"ok"s;
	if (status != 0)
	{
		if (status == NATS_TIMEOUT)
		{
			status_msg = L"timeout"s;
		}
		else if (status == NATS_NO_RESPONDERS)
		{
			status_msg = L"no responsers"s;
		}
		else
		{
			status_msg = std::to_wstring(status);
		}
	}
	else
	{
		// empty
	}

	return status_msg;
}

void _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
#ifndef SERVICE_TEST
	if (!is_service_running())
	{
		DEBUG_STRING(L"@rg TimerCallback: maybe service paused.\n");
		return;
	}
#endif

	auto nc_ptr = reinterpret_cast<freeze::nats_client *>(lpArgToCompletionRoutine);
	if (!nc_ptr)
	{
		DEBUG_STRING(L"@rg TimerCallback: args is null.\n");
		return;
	}

	// TODO: check tp-process running.
	HANDLE hprocess = nullptr;
	HANDLE hthread = nullptr;
	auto pid = query_process_id();
	if (!pid)
	{
		DEBUG_STRING(L"@rg TimerCallback: pid is null, try re-open process.\n");
		pid = create_process_ex(hprocess, hthread);
	}

	if (pid)
	{
		DEBUG_STRING(L"@rg TimerCallback: process id={}, handle={}\n", pid, reinterpret_cast<int>(hprocess));
	}
	if (hprocess)
	{
		CloseHandle(hprocess);
	}
	if (hthread)
	{
		CloseHandle(hthread);
	}

	if (!nc_ptr->is_connected())
	{
		auto _ip = reset_ip_address();
		if (_ip < 0)
		{
			DEBUG_STRING(L"@rg TimerCallback: error remote ip is null, {}.\n"sv, reset_ip_error(_ip));
			return;
		}

		auto connected = nc_ptr->connect(_ip);
		if (!connected)
		{
			DEBUG_STRING(L"@rg TimerCallback error: remote not connected.\n");
			return;
		}
	}

	auto status_msg = heartbeat(nc_ptr, dwTimerLowValue);
	DEBUG_STRING(L"@rg TimerCallback: client pong: {}\n"sv, status_msg);
}

namespace freeze
{
	rgm_timer::rgm_timer()
		: _signal{}
	{
	}

	rgm_timer::~rgm_timer()
	{
	}

	void rgm_timer::start()
	{
	}

	void rgm_timer::stop()
	{
	}

	void rgm_timer::pause()
	{
	}

	void rgm_timer::resume()
	{
	}
}

namespace freeze
{
	void rgm_timer::on_network_connect()
	{
	}

	void rgm_timer::on_network_disconnect()
	{
	}
}
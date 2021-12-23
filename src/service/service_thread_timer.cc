//
// heartbeat timer
//

#include "service_thread_timer.h"
#include "service_nats_client.h"
#include "service_extern.h"

DWORD _g_latest_time = 0;
void _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
#ifndef SERVICE_TEST
	if (get_service_status() != freeze::service_state::running)
	{
		return;
	}
#endif

	auto nc_ptr = reinterpret_cast<freeze::nats_client*>(lpArgToCompletionRoutine);
	if (!nc_ptr)
	{
		DEBUG_STRING(L"@rg TimerCallback: args is null.\n");
		return;
	}

	auto _timeout = static_cast<DWORD>((dwTimerLowValue - _g_latest_time) * 1e-7);
	// auto _timout = LARGE_INTEGER{ dwTimerLowValue, (LONG)dwTimerHighValue }.QuadPart;
	DEBUG_STRING(L"@rg TimerCallback: ping ETA: {}\n."sv, _timeout);
	_g_latest_time = dwTimerLowValue;
	auto status = nc_ptr->_maybe_heartbeat();
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
	DEBUG_STRING(L"@rg TimerCallback: service pong: {}\n."sv, status_msg);
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
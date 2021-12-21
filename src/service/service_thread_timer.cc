#include "service_thread_timer.h"
#include "service_nats_client.h"

DWORD _g_latest_time = 0;
void _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
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
	auto ok = nc_ptr->_maybe_heartbeat();
	DEBUG_STRING(L"@rg TimerCallback: service pong: {}\n."sv, ok);
}

namespace freeze
{
	rgm_timer::rgm_timer()
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

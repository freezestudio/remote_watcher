#include "service_thread_timer.h"
#include "service_nats_client.h"

void _TimerCallback(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	auto ncp = reinterpret_cast<freeze::nats_client*>(lpArgToCompletionRoutine);
	if (!ncp)
	{
		DEBUG_STRING(L"@rg TimerCallback: args is null.\n");
		return;
	}

	// auto _timout = LARGE_INTEGER{ dwTimerLowValue, (LONG)dwTimerHighValue }.QuadPart;
	DEBUG_STRING(L"@rg TimerCallback: ETA: {}\n."sv, dwTimerLowValue);
}

#include "service_thread_timer.h"
#include "service_nats_client.h"

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

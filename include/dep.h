//
// common head files
//

// Windows

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// STL

#include <string>
#include <format>
#include <filesystem>

#define SERVICE_NAME L"rgmsvc"

#define SERVICE_CONTROL_NETWORK_CONNECT    129
#define SERVICE_CONTROL_NETWORK_DISCONNECT 130


namespace freeze
{
	//SERVICE_STOPPED                        0x00000001
	//SERVICE_START_PENDING                  0x00000002
	//SERVICE_STOP_PENDING                   0x00000003
	//SERVICE_RUNNING                        0x00000004
	//SERVICE_CONTINUE_PENDING               0x00000005
	//SERVICE_PAUSE_PENDING                  0x00000006
	//SERVICE_PAUSED                         0x00000007
	enum class service_state
	{
		stopped = SERVICE_STOPPED,
		start_pending = SERVICE_START_PENDING,
		stop_pending = SERVICE_STOP_PENDING,
		running = SERVICE_RUNNING,
		continue_pending = SERVICE_CONTINUE_PENDING,
		pause_pending = SERVICE_PAUSE_PENDING,
		paused = SERVICE_PAUSED,
    };

	enum class service_control
	{
		stop = SERVICE_CONTROL_STOP,
		pause = SERVICE_CONTROL_PAUSE,
		_continue = SERVICE_CONTROL_CONTINUE,
		shutdown = SERVICE_CONTROL_SHUTDOWN,
		param_change = SERVICE_CONTROL_PARAMCHANGE,
		network_connect = SERVICE_CONTROL_NETWORK_CONNECT,
		network_disconnect = SERVICE_CONTROL_NETWORK_DISCONNECT,
	};

	enum class service_accept
	{
		stop = SERVICE_ACCEPT_STOP,
		pause_continue = SERVICE_ACCEPT_PAUSE_CONTINUE,
		shutdown = SERVICE_ACCEPT_SHUTDOWN,
		param_change = SERVICE_ACCEPT_PARAMCHANGE,
	};
}
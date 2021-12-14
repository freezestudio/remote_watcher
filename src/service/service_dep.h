//
// service dependence head files
//
#ifndef SERVICE_DEP_H
#define SERVICE_DEP_H

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <fstream>
#include <filesystem>

// out namespaces
using namespace std::literals;
namespace fs = std::filesystem;

#define SERVICE_CONTROL_NETWORK_CONNECT    129
#define SERVICE_CONTROL_NETWORK_DISCONNECT 130

template<typename T>
concept ServiceEnum = std::is_enum_v<T>;

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
		network_connect = SERVICE_CONTROL_NETWORK_CONNECT,
		network_disconnect = SERVICE_CONTROL_NETWORK_DISCONNECT,
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

template<ServiceEnum T>
constexpr auto to_dword(T&& e)
{
	return static_cast<DWORD>(e);
}

template<ServiceEnum T>
constexpr T to_enum(DWORD d)
{
	return static_cast<T>(d);
}

template<typename B, typename E>
	requires ServiceEnum<B> || ServiceEnum<E> || std::same_as<DWORD, B> || std::same_as<DWORD, E>
constexpr DWORD operator|(B && t1, E && t2)
{
	if constexpr (std::is_enum_v<B>)
	{
		if constexpr (std::is_enum_v<E>)
		{
			return to_dword(t1) | to_dword(t2);
		}
		else
		{
			return to_dword(t1) | t2;
		}
	}
	else
	{
		if constexpr (std::is_enum_v<E>)
		{
			return t1 | to_dword(t2);
		}
		else
		{
			return t1 | t2;
		}
	}
}

namespace freeze::detail
{
	struct notify_information_w
	{
		LARGE_INTEGER size;
		LARGE_INTEGER creation;
		LARGE_INTEGER modification;
		LARGE_INTEGER change;
		DWORD attributes;
		bool folder;
		DWORD action; // set to 1,2,5
		std::wstring filename;
	};

	// test only! global data should used in golbal thread.
	extern std::vector<notify_information_w> g_local_notify_info_w;

	std::vector<notify_information_w>& get_changed_information();
}

constexpr auto sync_reason_none__reason = 0L;
constexpr auto sync_reason_recv_command = 1L;
constexpr auto sync_reason_recv_message = 2L;
constexpr auto sync_reason_send_command = 3L;
constexpr auto sync_reason_send_message = 4L;
constexpr auto sync_reason_send_payload = 5L;

namespace freeze
{
	class atomic_sync
	{
	public:
		atomic_sync()
		{
			_flag.clear();
		}

		~atomic_sync()
		{

		}

		atomic_sync(atomic_sync const&) = delete;
		atomic_sync& operator=(atomic_sync const&) = delete;
		atomic_sync(atomic_sync&&) = delete;
		atomic_sync& operator=(atomic_sync&&) = delete;

	public:
		void wait()
		{
			_flag.wait(false);
			_flag.clear();
		}

		void notify(bool all = false)
		{
			_flag.test_and_set();
			if (all)
			{
				_flag.notify_all();
			}
			else
			{
				_flag.notify_one();
			}
		}

	private:
		std::atomic_flag _flag;
	};

	class atomic_sync_reason
	{
	public:
		atomic_sync_reason()
		{
			reset();
		}

	public:
		long wait_reason()
		{
			_reason.wait(sync_reason_none__reason);
			return reset();
		}

		long notify_reason(long reason, bool all = false)
		{
			until_wait(reason);
			if (all)
			{
				_reason.notify_all();
			}
			else
			{
				_reason.notify_one();
			}
			return _reason.load();
		}

	private:
		bool until_wait(long reason)
		{
			auto expected = sync_reason_none__reason;
			while (!_reason.compare_exchange_strong(expected, reason))
			{
				expected = sync_reason_none__reason;
			}
			return true;
		}

		long reset()
		{
			return _reason.exchange(sync_reason_none__reason);
		}
	private:
		std::atomic_long _reason;
	};
}

extern freeze::atomic_sync_reason global_reason_signal;

#endif
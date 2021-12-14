#ifndef SERVICE_NATS_CLIENT_H
#define SERVICE_NATS_CLIENT_H

#include "common_dep.h"
#include "service_dep.h"

#include "nats.h"
#include "json.hpp"

namespace freeze::detail
{
	struct _nats_connect;
	struct nats_cmd
	{
		std::string name;
		std::string action;
	};

	constexpr nats_cmd make_cmd(std::string const& name, std::string const& action)
	{
		return { name, action };
	}

	inline nats_cmd to_cmd(char const* str, std::size_t len)
	{
		using json = nlohmann::json;
		auto j = json::parse(str, str + len);
		return {
			j["name"],
			j["action"]
		};

	}

	inline nats_cmd to_cmd(std::string const& str)
	{
		return to_cmd(str.c_str(), str.size());
	}

	inline std::string from_cmd(nats_cmd const& cmd)
	{
		using json = nlohmann::json;
		json j;
		j["name"] = cmd.name;
		j["action"] = cmd.action;
		return j.dump();
	}
}

namespace freeze
{
	constexpr auto message_channel = "message-channel"sv;
	constexpr auto command_channel = "command-channel"sv;
	constexpr auto payload_channel = "payload-channel"sv;
	constexpr auto json_type = "json"sv;
	constexpr auto data_type = "data"sv;
	constexpr auto text_type = "text"sv;

	class nats_client
	{
	public:
		nats_client();
		~nats_client();

	public:
		void change_ip(DWORD ip, std::string const & = {});
		bool connect(DWORD ip, std::string const& = {});
		void close();

	public:
		void listen_message();
		void listen_command();

	public:
		void notify_message();
		void notify_command();
		void notify_payload();

	public:
		void on_message();
		void on_command();
		void on_payload();

	private:
		std::unique_ptr<detail::_nats_connect> pimpl;

	private:
		std::thread _msg_thread;
		std::thread _cmd_thread;
		bool _msg_thread_running{false};
		bool _cmd_thread_running{false};
	};
}

#endif

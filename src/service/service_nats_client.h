#ifndef SERVICE_NATS_CLIENT_H
#define SERVICE_NATS_CLIENT_H

#include "dep.h"
#include "service_dep.h"
#include "nats.h"
#include "json.hpp"

namespace freeze::detail
{
	struct _nats;

	struct _nats_cmd
	{
		std::string name;
		std::string action;
	};

	constexpr _nats_cmd make_cmd(std::string const& name, std::string const& action)
	{
		return { name, action };
	}

	inline _nats_cmd to_cmd(char const* str, std::size_t len)
	{
		using json = nlohmann::json;
		auto j = json::parse(str, str + len);
		return {
			j["name"],
			j["action"]
		};

	}

	inline _nats_cmd to_cmd(std::string const& str)
	{
		return to_cmd(str.c_str(), str.size());
	}

	inline std::string from_cmd(_nats_cmd const& cmd)
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

	class nats_client
	{
	public:
		nats_client();
		~nats_client();
	public:
		void change_ip(DWORD ip);
		void connect(std::string const& = {});
		void close();

	public:
		void notify_message();
		void notify_command();

	private:
		std::unique_ptr<detail::_nats> piml;
	};
}

#endif

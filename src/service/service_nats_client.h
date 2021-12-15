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

	struct nats_ack
	{
		std::string name;
		bool result;
	};

	struct nats_cmd_ack : nats_ack
	{
		std::string action;
	};

	struct nats_pal_ack : nats_ack
	{
		uintmax_t size;
	};

	constexpr nats_cmd make_cmd(std::string const& name, std::string const& action)
	{
		return { name, action };
	}

	inline nats_cmd to_cmd(char const* str, std::size_t len)
	{
		using json = nlohmann::json;
		auto j = json::parse(str, str + len);
		if (j.is_null())
		{
			return {};
		}

		if (j.find("name") == j.end() || j.find("action") == j.end())
		{
			return {};
		}

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

	inline nats_cmd_ack to_cmd_ack(char const* ack, std::size_t len)
	{
		using json = nlohmann::json;
		auto j = json::parse(ack, ack + len);
		nats_cmd_ack _nca;
		_nca.name = j["name"];
		_nca.result = j["result"];
		_nca.action = j["action"];
		return _nca;
	}

	inline nats_cmd_ack to_cmd_ack(std::string const& ack)
	{
		return to_cmd_ack(ack.c_str(), ack.size());
	}

	inline std::string from_cmd_ack(nats_cmd_ack const& ack)
	{
		using json = nlohmann::json;
		json j;
		j["name"] = ack.name;
		j["action"] = ack.action;
		j["result"] = ack.result;
		return j.dump();
	}

	inline nats_pal_ack to_pal_ack(char const* ack, std::size_t len)
	{
		using json = nlohmann::json;
		auto j = json::parse(ack, ack + len);
		nats_pal_ack _npa;
		_npa.name = j["name"];
		_npa.result = j["result"];
		_npa.size = j["size"];
			return _npa;
	}

	inline nats_pal_ack to_pal_ack(std::string const& ack)
	{
		return to_pal_ack(ack.c_str(), ack.size());
	}

	inline std::string from_pal_ack(nats_pal_ack const& ack)
	{
		using json = nlohmann::json;
		json j;
		j["name"] = ack.name;
		j["size"] = ack.size;
		j["result"] = ack.result;
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
		void change_ip(DWORD ip, std::string const& = {});
		bool connect(DWORD ip, std::string const& = {});
		void close();

	public:
		void listen_message();
		void listen_command();

	public:
		void notify_message();
		void notify_command();
		void notify_payload(fs::path const&, std::vector<detail::notify_information_w> const&);

	public:
		void on_message();
		void on_command();

	public:
		//void on_payload_response();

	private:
		std::unique_ptr<detail::_nats_connect> pimpl;

	private:
		std::thread _msg_thread;
		std::thread _cmd_thread;
		//std::thread _pal_thread;
		bool _msg_thread_running{ true };
		bool _cmd_thread_running{ true };
		//bool _pal_thread_running{ true };
	};
}

#endif

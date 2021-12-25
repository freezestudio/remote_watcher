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

	struct interactive_message
	{
		std::string name;
	};

	struct nats_recv_message : interactive_message
	{
		std::string folder;
		// std::vector<std::string> ignores;
	};

	struct nats_send_message : interactive_message
	{
		std::vector<std::string> folders;
	};

	inline nats_recv_message parse_recv_message(std::string const& msg)
	{
		nats_recv_message nrm;
		if (!msg.empty())
		{
			// response={ name, folder? }
			using json = nlohmann::json;
			auto j = json::parse(msg);
			std::string name = j["name"];
			std::string folder;
			auto iter = j.find("folder");
			if (iter != j.end())
			{
				folder = j["folder"];
			}
			nrm.name = name;
			nrm.folder = folder;
		}
		return nrm;
	}

	inline constexpr nats_send_message make_send_message(std::string const& name, std::vector<std::string> const& folders)
	{
		nats_send_message nsm;
		nsm.name = name;
		nsm.folders = folders;
		return nsm;
	}

	inline std::string from_send_message(nats_send_message const& msg)
	{
		std::string result;
		if (!msg.name.empty())
		{
			using json = nlohmann::json;
			json j;
			j["name"] = msg.name;
			j["folders"] = msg.folders;
			result = j.dump();
		}
		return result;
	}

	inline std::string make_send_message_string(std::string const& name, std::vector<std::string> const& folders)
	{
		auto msg = make_send_message(name, folders);
		return from_send_message(msg);
	}

	inline constexpr nats_cmd make_cmd(std::string const& name, std::string const& action)
	{
		nats_cmd nc;
		nc.name = name;
		nc.action = action;
		return nc;
	}

	inline nats_cmd to_cmd(char const* str, std::size_t len)
	{
		nats_cmd nc;
		if (str != nullptr && len > 0)
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

			nc.name = j["name"];
			nc.action = j["action"];
		}		
		return nc;
	}

	inline nats_cmd to_cmd(std::string const& str)
	{
		return to_cmd(str.c_str(), str.size());
	}

	inline std::string from_cmd(nats_cmd const& cmd)
	{
		std::string result;
		if (!cmd.name.empty())
		{
			using json = nlohmann::json;
			json j;
			j["name"] = cmd.name;
			j["action"] = cmd.action;
			result = j.dump();
		}
		return result;
	}

	inline nats_cmd_ack to_cmd_ack(char const* ack, std::size_t len)
	{
		nats_cmd_ack _nca;
		if (ack != nullptr && len > 0)
		{
			using json = nlohmann::json;
			auto j = json::parse(ack, ack + len);
			_nca.name = j["name"];
			_nca.result = j["result"];
			_nca.action = j["action"];
		}		
		return _nca;
	}

	inline nats_cmd_ack to_cmd_ack(std::string const& ack)
	{
		return to_cmd_ack(ack.c_str(), ack.size());
	}

	inline std::string from_cmd_ack(nats_cmd_ack const& ack)
	{
		std::string result;
		if (!ack.name.empty())
		{
			using json = nlohmann::json;
			json j;
			j["name"] = ack.name;
			j["action"] = ack.action;
			j["result"] = ack.result;
			result = j.dump();
		}
		return result;
	}

	inline nats_pal_ack to_pal_ack(char const* ack, std::size_t len)
	{
		nats_pal_ack _npa;
		if (ack != nullptr && len > 0)
		{
			using json = nlohmann::json;
			auto j = json::parse(ack, ack + len);
			_npa.name = j["name"];
			_npa.result = j["result"];
			_npa.size = j["size"];
		}
		return _npa;
	}

	inline nats_pal_ack to_pal_ack(std::string const& ack)
	{
		return to_pal_ack(ack.c_str(), ack.size());
	}

	inline std::string from_pal_ack(nats_pal_ack const& ack)
	{
		std::string result;
		if (!ack.name.empty())
		{
			using json = nlohmann::json;
			json j;
			j["name"] = ack.name;
			j["size"] = ack.size;
			j["result"] = ack.result;
			result = j.dump();
		}
		return result;
	}
}

namespace freeze
{
	constexpr auto message_recv_channel = "message-channel-1"sv;
	constexpr auto message_send_channel = "message-channel-2"sv;
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
		void notify_message();
		std::string notify_command();
		void notify_payload(fs::path const&);

	public:
		void send_payload();
		void command_handle_result();
		void message_response();

	public:
		DWORD _maybe_heartbeat();

	private:
		void init_threads();
		void stop_threads();

	private:
		std::unique_ptr<detail::_nats_connect> pimpl;

	private:
		std::thread _msg_thread;
		std::thread _cmd_thread;
		std::thread _pal_thread;

		bool _msg_thread_running{ true };
		bool _cmd_thread_running{ true };
		bool _pal_thread_running{ true };

		atomic_sync _message_signal{};
		atomic_sync _command_signal{};
		atomic_sync _payload_signal{};

	private:
		atomic_sync _cmd_response_signal{};
		std::string _response_command;

	private:
		mutable std::mutex _mutex;
		fs::path _watch_path;
	};
}

#endif

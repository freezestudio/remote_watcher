#ifndef SERVICE_NATS_CLIENT_H
#define SERVICE_NATS_CLIENT_H

#include "common_dep.h"
#include "service_dep.h"
#include "service_utils.h"

#include "nats.h"
#include "json.hpp"

#define LABEL_NAME(label) label
#define MSG_LIST_DISK LABEL_NAME("list-disk")
#define MSG_LIST_DIR LABEL_NAME("select-directory")
#define MSG_LIST_FILE LABEL_NAME("select-files")
#define MSG_TREE_INFO LABEL_NAME("tree-info")
#define MSG_SYNC_FILES LABEL_NAME("sync-files")
#define MSG_FOLDER LABEL_NAME("watch-folder")
#define CMD_FOLDER LABEL_NAME("modify-folder")
#define CMD_IGNORE LABEL_NAME("modify-ignores")

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
		// TODO: add ignores
		std::vector<std::string> ignores;
	};

	struct nats_disk_message : interactive_message
	{
		std::vector<std::string> disks;
	};

	struct nats_folder_message : interactive_message
	{
		std::vector<std::string> folders;
	};

	struct nats_file_message : interactive_message
	{
		std::vector<std::string> files;
	};

	using nats_watch_message = nats_folder_message;
	using nats_send_message = nats_folder_message;

	inline nats_recv_message parse_recv_message(std::string const &msg)
	{
		nats_recv_message nrm;
		if (!msg.empty())
		{
			// response={ name, folder? }
			using json = nlohmann::json;
			auto j = json::parse(msg);
			std::string name = j["name"];
			std::string folder = "";
			std::vector<std::string> ignores;
			auto fiter = j.find("folder");
			if (fiter != j.end())
			{
				folder = j["folder"];
			}
			auto iiter = j.find("ignores");
			if (iiter != j.end())
			{
				for (auto const& s : j["ignores"])
				{
					ignores.push_back(s);
				}
			}

			nrm.name = name;
			nrm.folder = folder;
			nrm.ignores = ignores;
		}
		else
		{
			nrm.name = "";
			nrm.folder = "";
			nrm.ignores = {};
		}
		return nrm;
	}

	inline std::string make_send_message_string(std::string const &name, std::vector<std::string> const &data)
	{
		// enum class value_t : std::uint8_t
		//{
		//	null,             ///< null value
		//	object,           ///< object (unordered set of name/value pairs)
		//	array,            ///< array (ordered collection of values)
		//	string,           ///< string value
		//	boolean,          ///< boolean value
		//	number_integer,   ///< number value (signed integer)
		//	number_unsigned,  ///< number value (unsigned integer)
		//	number_float,     ///< number value (floating-point)
		//	binary,           ///< binary array (ordered collection of bytes)
		//	discarded         ///< discarded by the parser callback function
		// };

		std::string result;
		using json = nlohmann::json;
		json j;
		j["name"] = name;
		if (!name.empty())
		{
			if (name == MSG_LIST_DISK)
			{
				j["disks"] = data;
			}
			else if (name == MSG_LIST_DIR)
			{
				j["folders"] = data;
			}
			else if (name == MSG_LIST_FILE)
			{
				j["files"] = data;
			}
			else if (name == MSG_FOLDER)
			{
				j["folders"] = data;
			}
			else
			{
				// more.
			}
		}
		result = j.dump();
		return result;
	}

	inline std::string make_send_message_string(std::string const &name, std::vector<detail::tree_information> const &vec_info)
	{
		if (name != MSG_TREE_INFO)
		{
			return {};
		}
		std::string result;
		using json = nlohmann::json;
		json jt;
		jt["name"] = name;
		// jt["info"] = {
		//   {{"file_path","path-a"}, {"file_name", "name-a"}, {"file_size", 11}},
		//   {{"file_path","path-b"}, {"file_name", "name-b"}, {"file_size", 22}},
		// };
		jt["info"] = json::array();
		for (auto const& _i : vec_info)
		{
			jt["info"].push_back(json::object({{"file_path", _i.file_path}, {"file_name", _i.file_name}, {"file_size", _i.file_size}}));
		}
		result = jt.dump();
		return result;
	}

	inline constexpr nats_cmd make_cmd(std::string const &name, std::string const &action)
	{
		nats_cmd nc;
		nc.name = name;
		nc.action = action;
		return nc;
	}

	inline nats_cmd to_cmd(char const *str, std::size_t len)
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

	inline nats_cmd to_cmd(std::string const &str)
	{
		return to_cmd(str.c_str(), str.size());
	}

	inline std::string from_cmd(nats_cmd const &cmd)
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

	inline nats_cmd_ack to_cmd_ack(char const *ack, std::size_t len)
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

	inline nats_cmd_ack to_cmd_ack(std::string const &ack)
	{
		return to_cmd_ack(ack.c_str(), ack.size());
	}

	inline std::string from_cmd_ack(nats_cmd_ack const &ack)
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

	inline nats_pal_ack to_pal_ack(char const *ack, std::size_t len)
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

	inline nats_pal_ack to_pal_ack(std::string const &ack)
	{
		return to_pal_ack(ack.c_str(), ack.size());
	}

	inline std::string from_pal_ack(nats_pal_ack const &ack)
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

// wait for command response value
// extern freeze::atomic_sync g_cmd_response_signal;
// command response value
extern std::string g_cmd_response;

namespace freeze
{
	constexpr auto message_recv_channel = "message-channel-1"sv; // note: need exchange client channel
	constexpr auto message_send_channel = "message-channel-2"sv;
	constexpr auto command_channel = "command-channel"sv;
	constexpr auto payload_channel = "payload-channel"sv;
	constexpr auto synfile_recv_channel = "synfile-channel-1"sv; // note: need exchange client channel
	constexpr auto synfile_send_channel = "synfile-channel-2"sv;
	constexpr auto json_type = "json"sv;
	constexpr auto data_type = "data"sv; // unused
	constexpr auto text_type = "text"sv;
	// constexpr auto file_type = "file"sv;

	class nats_client
	{
	public:
		nats_client();
		~nats_client();

	public:
		void change_ip(DWORD ip, std::string const & = {});
		bool connect(DWORD ip, std::string const & = {});
		void close();
		bool is_connected();

	public:
		void notify_message();
		void notify_command();
		void notify_payload(fs::path const &);
		void notify_files();

	public:
		void send_payload();
		void command_handle_result();
		void message_response();
		void sync_files();

	public:
		DWORD maybe_heartbeat();

	private:
		void init_threads();
		void stop_threads();

	private:
		std::unique_ptr<detail::_nats_connect> pimpl;

	private:
		// TODO: use std::jthread
		std::thread _msg_thread;
		std::thread _cmd_thread;
		std::thread _pal_thread;
		std::thread _syn_thread;

		atomic_sync _message_signal{};
		atomic_sync _command_signal{};
		atomic_sync _payload_signal{};
		atomic_sync _synfile_signal{};

	private:
		bool _msg_thread_running{true};
		bool _cmd_thread_running{true};
		bool _pal_thread_running{true};
		bool _syn_thread_running{true};

	private:
		mutable std::mutex _mutex;
		fs::path _watch_path;
		fs::path _sync_path;
		std::vector<fs::path> _sync_igonres;
	};
}

#endif

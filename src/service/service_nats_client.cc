#include "service_nats_client.h"
#include "service_utils.h"

#include <atomic>

freeze::atomic_sync g_message_signal{};
freeze::atomic_sync g_command_signal{};
freeze::atomic_sync g_payload_signal{};

//std::mutex command_mutex;
//std::string g_current_command_reply;
freeze::detail::nats_cmd g_current_command;

namespace freeze::detail
{
	//constexpr void verify_status(natsStatus status)
	//{
	//	assert(status == natsStatus::NATS_OK);
	//}

	struct _nats_options
	{
	public:
		_nats_options(/*std::nullptr_t*/)
			: _options{ nullptr }
		{

		}

		_nats_options(
			std::string const& url,
			std::string const& user,
			std::string const& pwd,
			std::string const& name
		) : _nats_options(name)
		{
			_url(url);
			_user_pwd(user, pwd);
		}

		_nats_options(
			std::string const& url,
			std::string const& token,
			std::string const& name
		) : _nats_options(name)
		{
			_url(url);
			_token(token);
		}

		explicit _nats_options(std::string const& name)
			: _options{ nullptr }
		{
			_create();

			if (!name.empty())
			{
				_name(name);
			}
			_buffer_size(64 * 1024);
		}

		_nats_options(_nats_options const&) = default;
		_nats_options& operator=(_nats_options const&) = default;

		~_nats_options()
		{
			_destroy();
		}

		operator natsOptions* () const
		{
			return _options;
		}

	public:
		_nats_options& reset(std::string const& url, std::string const& token, std::string const& name = {})
		{
			_destroy();
			_create();
			if (!name.empty())
			{
				_name(name);
			}
			_buffer_size(64 * 1024);
			_url(url);
			_token(token);
			return *this;
		}

	public:
		bool set_url(std::string const& url)
		{
			return _url(url);
		}

		bool set_pass(std::string const& user, std::string const& pwd)
		{
			return _user_pwd(user, pwd);
		}

		bool set_token(std::string const& token)
		{
			return _token(token);
		}

	private:
		bool _create()
		{
			return natsOptions_Create(&_options) == natsStatus::NATS_OK;
		}

		void _destroy()
		{
			if (_options)
			{
				natsOptions_Destroy(_options);
				_options = nullptr;
			}
		}

		bool _name(std::string const& name)
		{
			return natsOptions_SetName(_options, name.c_str()) == natsStatus::NATS_OK;
		}

		bool _buffer_size(int size = 0)
		{
			// 0==default==32kb
			return natsOptions_SetIOBufSize(_options, size) == natsStatus::NATS_OK;
		}

		bool _user_pwd(std::string const& user, std::string const& pwd)
		{
			return natsOptions_SetUserInfo(_options, user.c_str(), pwd.c_str()) == natsStatus::NATS_OK;
		}

		bool _token(std::string const& token)
		{
			return natsOptions_SetToken(_options, token.c_str()) == natsStatus::NATS_OK;
		}

		bool _url(std::string const& url)
		{
			std::string nats_url;
			if (!url.starts_with("nats://"))
			{
				nats_url = "nats://";
			}
			nats_url += url;

			auto _find = std::find_if(std::cbegin(url), std::cend(url), [](auto&& c) {return c == ':'; });
			if (_find == url.end())
			{
				nats_url += ":4222";
			}
			return natsOptions_SetURL(_options, nats_url.c_str()) == natsStatus::NATS_OK;
		}

	private:
		natsOptions* _options = nullptr;
	};

	struct _nats_msg
	{
		_nats_msg(natsMsg* m)
			: _msg{ m }
		{
			if (m)
			{
				_sub = natsMsg_GetSubject(_msg);
			}
		}

		explicit _nats_msg(std::string const& sub, bool heartbeat = false)
			:_sub{ sub }
		{
			// as heartbeat package
			if (heartbeat)
			{
				_create_empty(sub);
			}
		}

		_nats_msg(std::string const& sub, std::string const& m)
			: _sub{ sub }
		{
			_create_text(m);
		}

		~_nats_msg()
		{
			_destroy();
		}

		operator natsMsg* () const
		{
			return _msg;
		}

		natsMsg** put()
		{
			return &_msg;
		}

	public:
		std::string get_reply()
		{
			if (!_msg)
			{
				return {};
			}
			auto _reply = natsMsg_GetReply(_msg);
			if (!_reply)
			{
				return {};
			}
			return _reply;
		}

	public:
		std::string get_msg()
		{
			auto data = natsMsg_GetData(_msg);
			auto len = natsMsg_GetDataLength(_msg);
			return std::string(data, len);
		}

		nats_cmd get_cmd()
		{
			auto cmd = get_msg();
			return detail::to_cmd(cmd);
		}

		nats_cmd_ack get_cmd_ack()
		{
			auto ack = get_msg();
			return detail::to_cmd_ack(ack);
		}

		nats_pal_ack get_pal_ack()
		{
			auto ack = get_msg();
			return detail::to_pal_ack(ack);
		}

		bool set_msg(std::string const& msg, std::string const& _type)
		{
			_destroy();
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, msg.c_str(), msg.size()) == NATS_OK;
			if (!ok)
			{
				return false;
			}
			const char* value = nullptr;
			auto status = natsMsgHeader_Get(_msg, "type", &value);
			if (status == NATS_OK)
			{
				if (text_type != std::string_view(value))
				{
					ok = _set_header("type", "text");
				}
			}
			else if (status == NATS_NOT_FOUND)
			{
				ok = _add_header("type", "text");
			}
			else
			{
				return false;
			}

			if (!ok)
			{
				return false;
			}

			if (json_type == _type)
			{
				ok = _set_header("type", "json");
			}
			return ok;
		}

		bool set_cmd(nats_cmd const& cmd)
		{
			_destroy();
			auto _s = from_cmd(cmd);
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, _s.data(), _s.size()) == NATS_OK;
			if (!ok)
			{
				return false;
			}
			ok = _clear_headers();
			if (!ok)
			{
				return false;
			}
			ok = _add_header("type", "json");

			// TODO: recv reply response.

			return ok;
		}

		bool set_cmd_ack(std::string const& reply, nats_cmd_ack const& ack)
		{
			_destroy();
			auto _s = from_cmd_ack(ack);
			auto ok = natsMsg_Create(&_msg, reply.c_str(), nullptr, _s.data(), _s.size()) == NATS_OK;
			if (!ok)
			{
				return false;
			}
			ok = _clear_headers();
			if (!ok)
			{
				return false;
			}
			ok = _add_header("type", "json");
			return ok;
		}

		// caller should release this pointer
		char* set_payload(fs::path const& folder, fs::path const& file)
		{
			auto full_path_file = (folder / file).lexically_normal();
			if (!fs::exists(full_path_file))
			{
				return nullptr;
			}

			auto len = fs::file_size(full_path_file);
			char* data = new char[len] {};
			std::ifstream ifs(full_path_file, std::ios::binary);
			if (ifs.is_open())
			{
				ifs.read(data, len);
				ifs.close();
				_destroy();
				auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, data, len) == NATS_OK;
				if (!ok)
				{
					goto fail;
				}
				ok = _clear_headers();
				if (!ok)
				{
					goto fail;
				}
				ok = _add_header("type", "data");
				if (!ok)
				{
					goto fail;
				}
				std::wstring filename = file.c_str();
				ok = _add_header("name", detail::to_utf8(filename.c_str(), filename.size()));
				if (!ok)
				{
					goto fail;
				}
				ok = _add_header("size", std::to_string(len));
				if (!ok)
				{
					goto fail;
				}
				auto prefix = "mime/"s;
				auto suffix = file.extension();
				auto str_suf = suffix.string();
				if (str_suf.starts_with("."))
				{
					prefix += str_suf.substr(1);
				}
				else
				{
					prefix += str_suf;
				}
				ok = _add_header("mime", prefix);
				if (!ok)
				{
					goto fail;
				}
				return data;
			}

		fail:
			delete[] data;
			return nullptr;
		}

	private:
		bool _auto_ack()
		{
			return natsMsg_Ack(_msg, nullptr) == NATS_OK;
		}

		bool _create_empty(std::string const& reply = {})
		{
			_destroy();
			auto rep = reply.empty() ? nullptr : reply.c_str();
			return natsMsg_Create(&_msg, _sub.c_str(), rep, nullptr, 0) == NATS_OK;
		}

		bool _create_text(std::string const& m)
		{
			_destroy();
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, m.data(), m.size()) == NATS_OK;
			if (!ok)
			{
				return false;
			}
			ok = _clear_headers();
			if (!ok)
			{
				return false;
			}
			ok = _add_header("type", "text");
			return ok;
		}

		std::string _create_reply_text(std::string const& reply, std::string const& m)
		{
			_destroy();
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), reply.c_str(), m.data(), m.size()) == NATS_OK;
			if (!ok)
			{
				return {};
			}
			ok = _clear_headers();
			if (!ok)
			{
				return {};
			}
			ok = _add_header("type", "text");
			if (!ok)
			{
				return {};
			}

			return reply;
		}

		bool _set_header(std::string key, std::string const& value)
		{
			return natsMsgHeader_Set(_msg, key.c_str(), value.c_str()) == NATS_OK;
		}

		bool _add_header(std::string key, std::string const& value)
		{
			return natsMsgHeader_Add(_msg, key.c_str(), value.c_str()) == NATS_OK;
		}

		bool _clear_headers()
		{
			const char** keys = nullptr;
			int count = 0;
			auto status = natsMsgHeader_Keys(_msg, &keys, &count);
			if (status == NATS_NOT_FOUND)
			{
				return true;
			}
			auto ok = status == NATS_OK;
			if (!ok)
			{
				return false;
			}

			for (int i = 0; i < count; ++i)
			{
				auto key = keys[i];
				ok = natsMsgHeader_Delete(_msg, key) == NATS_OK;
				if (!ok)
				{
					break;
				}
			}

			free((void*)keys);
			return ok;
		}

		void _destroy()
		{
			if (_msg)
			{
				natsMsg_Destroy(_msg);
				_msg = nullptr;
			}
		}

	private:
		std::string _sub;
		natsMsg* _msg = nullptr;
	};

	struct _nats_sub
	{
		_nats_sub(natsSubscription* sub)
			: _sub{ sub }
		{

		}

		natsSubscription** put()
		{
			return &_sub;
		}

	public:
		bool auto_unsub(int max)
		{
			return natsSubscription_AutoUnsubscribe(_sub, max) == NATS_OK;
		}

		_nats_msg next_msg(int64_t timeout)
		{
			natsMsg* m;
			auto ok = natsSubscription_NextMsg(&m, _sub, timeout) == NATS_OK;
			if (ok)
			{
				return _nats_msg{ m };
			}
			return nullptr;
		}

	private:
		bool _destroy()
		{
			natsSubscription_Destroy(_sub);
		}
	private:
		natsSubscription* _sub;
	};

	struct _nats_connect
	{
		// TODO: make default name = {nats-pcname-pid}
	public:
		_nats_connect(/*std::nullopt_t*/) noexcept
			: _opts{}
			, _nc{ nullptr }
		{

		}

		_nats_connect(
			std::string const& url,
			std::string const& user,
			std::string const& pwd,
			std::string const& name = {}
		) noexcept
			: _opts(url, user, pwd, name)
		{
			_connect();
		}

		_nats_connect(
			std::string const& url,
			std::string const& token,
			std::string const& name = {}
		) noexcept
			: _opts(url, token, name)
		{
			_connect();
		}

		~_nats_connect() noexcept
		{
			_destroy();
		}

	public:
		void close()
		{
			_destroy();
		}

		void change_ip(DWORD ip, std::string const& token /*= {}*/)
		{
			auto str_ip = detail::parse_ip_address(ip);
			if (is_connected())
			{
				if (remote_ip() == str_ip)
				{
					return;
				}
				reset(str_ip, token);
			}
		}

		_nats_connect& reset(std::string const& url, std::string const& token, std::string const& name = {})
		{
			auto _token = token;
			if (token.empty())
			{
				// TODO: read from .ini
				_token = "aH7g8Rxq0q"s;
			}

			_destroy();
			_opts.reset(url, _token, name);
			_connect();
			return *this;
		}

		bool ack_command(std::string const& reply, nats_cmd_ack const& ack)
		{
			auto _m = _nats_msg{ command_channel.data() };
			auto ok = _m.set_cmd_ack(reply, ack);
			if (!ok)
			{
				return false;
			}
			ok = natsConnection_PublishMsg(_nc, _m) == NATS_OK;
			return ok;
		}

	public:
		bool publish_message(std::string const& msg, std::string const& _type = std::string(text_type))
		{
			auto _m = _nats_msg{ message_channel.data() };
			if (_m.set_msg(msg, _type))
			{
				return natsConnection_PublishMsg(_nc, _m) == NATS_OK;
			}
			return false;
		}

		bool publish_command(nats_cmd const& cmd)
		{
			auto _m = _nats_msg{ command_channel.data() };
			_m.set_cmd(cmd);
			return natsConnection_PublishMsg(_nc, _m) == NATS_OK;
		}

		bool publish_payload(fs::path const& folder, fs::path const& file)
		{
			_nats_msg m{ payload_channel.data() };
			auto pdata = m.set_payload(folder, file);
			if (!pdata)
			{
				return false;
			}

			_nats_sub _sub{ nullptr };
			auto ok = natsConnection_Subscribe(_sub.put(), _nc, payload_channel.data(),
				[](natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure)
				{
					_nats_msg m{ msg };
					auto self = reinterpret_cast<_nats_connect*>(closure);
					auto str_msg = m.get_msg();
					self->on_payload_response(str_msg);
				}, this) == NATS_OK;
			if (!ok)
			{
				goto theend;
			}

			//no head: natsConnection_Publish(_nc, subj, data, len);
			ok = natsConnection_PublishMsg(_nc, m) == NATS_OK;

		theend:
			delete pdata;
			return ok;
		}

		bool subject_message()
		{
			_nats_sub _sub{ nullptr };
			auto status = natsConnection_Subscribe(_sub.put(), _nc, message_channel.data(),
				[](natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure)
				{
					_nats_msg m{ msg };
					auto self = reinterpret_cast<_nats_connect*>(closure);
					auto str_msg = m.get_msg();
					self->on_message(str_msg);
				}, this);
			return status == NATS_OK;
		}

		// TODO: use coroutine
		bool subject_command()
		{
			_nats_sub _sub{ nullptr };
			// cb(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
			auto status = natsConnection_Subscribe(_sub.put(), _nc, command_channel.data(),
				[](natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure)
				{
					//std::lock_guard<std::mutex> lock(command_mutex);
					_nats_msg m{ msg };
					//g_current_command_reply = m.get_reply();
					auto self = reinterpret_cast<_nats_connect*>(closure);
					auto cmd = m.get_cmd();
					detail::nats_cmd_ack _ack;
					_ack.name = cmd.name;
					_ack.action = cmd.action;
					_ack.result = true;
					self->ack_command(m.get_reply(), _ack);
					self->on_command(/*g_current_command_reply,*/ cmd);
				}, this);
			return status == NATS_OK;
		}

		std::string subject_response()
		{
			return {};
		}

	public:
		std::string remote_ip()
		{
			char ip[16]{};
			auto status = natsConnection_GetConnectedServerId(_nc, ip, 16);
			if (status == NATS_OK)
			{
				return ip;
			}
			else
			{
				return {};
			}
		}

		_nats_connect& connect(std::string const& url, std::string const& token, std::string const& name = {})
		{
			auto _token = "aH7g8Rxq0q"s;
			_opts.reset(url, _token, name);
			_connect();
			return *this;
		}

		bool is_connected()
		{
			return natsConnection_Status(_nc) == NATS_CONN_STATUS_CONNECTED;
		}

	public:
		bool drain()
		{
			return natsConnection_Drain(_nc) == NATS_OK;
		}

		bool is_draining()
		{
			return natsConnection_IsDraining(_nc);
		}

		bool flush()
		{
			return natsConnection_Flush(_nc) == NATS_OK;
		}

	public:
		std::string current_status()
		{
			std::string strs;
			if (!_nc)
			{
				strs = "null-connect"s;
				return strs;
			}

			auto status = natsConnection_Status(_nc);
			switch (status)
			{
			default:
				break;
			case NATS_CONN_STATUS_DISCONNECTED: strs = "disconnected"s; break;
			case NATS_CONN_STATUS_CONNECTING: strs = "connecting"s; break;
			case NATS_CONN_STATUS_CONNECTED: strs = "connected"s; break;
			case NATS_CONN_STATUS_CLOSED: strs = "closed"s; break;
			case NATS_CONN_STATUS_RECONNECTING: strs = "re-connecting"s; break;
			case NATS_CONN_STATUS_DRAINING_SUBS: strs = "draining-subscriptions"s; break;
			case NATS_CONN_STATUS_DRAINING_PUBS:strs = "draining-publishers"s; break;
			}
			return strs;
		}

		std::string error_message()
		{
			const char* error = nullptr;
			auto status = natsConnection_GetLastError(_nc, &error);
			if (status != NATS_OK)
			{
				return {};
			}

			std::string strerr = error;
			return strerr;
		}

	public:
		void on_message(std::string const& msg)
		{
			auto wstr = detail::to_utf16(msg);
			OutputDebugString(wstr.c_str());
			g_message_signal.notify();
		}

		void on_command(/*std::string const& reply,*/ nats_cmd const& cmd)
		{
			g_current_command.name = cmd.name;
			g_current_command.action = cmd.action;

			auto _a = std::format("command: name={}, action={}\n"sv, g_current_command.name, g_current_command.action);
			OutputDebugStringA(_a.c_str());

			g_command_signal.notify();
		}

		void on_payload_response(std::string const& msg)
		{
			g_payload_signal.notify();
		}

	private:
		bool _connect()
		{
			return natsConnection_Connect(&_nc, _opts) == NATS_OK;
		}

		void _disconnect()
		{
			if (!natsConnection_IsClosed(_nc))
			{
				natsConnection_Close(_nc);
			}
		}

		void _destroy()
		{
			_disconnect();
			natsConnection_Destroy(_nc);
			_nc = nullptr;
		}

	private:
		natsConnection* _nc = nullptr;
		_nats_options _opts;
	};
}

namespace freeze
{
	nats_client::nats_client()
		: pimpl{ std::make_unique<detail::_nats_connect>() }
	{
		_msg_thread = std::thread([](auto&& self)
			{
				while (true)
				{
					g_message_signal.wait();
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						break;
					}
					self_ptr->on_message();
					if (!(self_ptr->_msg_thread_running))
					{
						break;
					}
				}
			}, this);
		_cmd_thread = std::thread([](auto&& self)
			{
				while (true)
				{
					g_command_signal.wait();
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						break;
					}
					self_ptr->on_command();
					if (!(self_ptr->_cmd_thread_running))
					{
						break;
					}
				}
			}, this);
		_pal_thread = std::thread([](auto&& self)
			{
				while (true)
				{
					g_payload_signal.wait();
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						break;
					}
					self_ptr->on_payload_response();
					if (!(self_ptr->_pal_thread_running))
					{
						break;
					}
				}
			}, this);
	}

	nats_client::~nats_client()
	{
		_msg_thread_running = false;
		g_message_signal.notify();
		if (_msg_thread.joinable())
		{
			_msg_thread.join();
		}

		_cmd_thread_running = false;
		g_message_signal.notify();
		if (_cmd_thread.joinable())
		{
			_cmd_thread.join();
		}

		_pal_thread_running = false;
		g_payload_signal.notify();
		if (_pal_thread.joinable())
		{
			_pal_thread.join();
		}
	}

	void nats_client::change_ip(DWORD ip, std::string const& token /*= {}*/)
	{
		pimpl->change_ip(ip, token);
	}

	bool nats_client::connect(DWORD ip, std::string const& token/* = {}*/)
	{
		auto url = detail::parse_ip_address(ip);
		//pimpl.swap(detail::_nats_connect(url, token));
		pimpl->connect(url, token);
		return pimpl->is_connected();
	}

	void nats_client::close()
	{
		pimpl->close();
	}

	void nats_client::listen_message()
	{
		if (pimpl->subject_message())
		{
			_msg_thread_running = true;
		}
		else
		{
			_msg_thread_running = false;
			g_message_signal.notify();
		}
	}

	void nats_client::listen_command()
	{
		if (pimpl->subject_command())
		{
			_cmd_thread_running = true;
		}
		else
		{
			_cmd_thread_running = false;
			g_command_signal.notify();
		}
	}

	void nats_client::notify_message()
	{

	}

	void nats_client::notify_command()
	{
		// response
	}

	void nats_client::notify_payload(fs::path const& folder, std::vector<detail::notify_information_w> const& info)
	{
		auto _f = std::format(L"watcher: {}\n"sv, folder.c_str());
		OutputDebugString(_f.c_str());

		// TODO: async send image data step by step
		// ...

		for (auto& d : info)
		{
			auto _msg = std::format(L"action={}, name={}\n"sv, d.action, d.filename);
			OutputDebugString(_msg.c_str());

			// send to remote ...
			// ...
		}
	}

	void nats_client::on_command()
	{
		// switch(known cmd)
		// ...

		//std::lock_guard<std::mutex> lock(command_mutex);
		//// ack command: {name: cmdname, action: string, result: xxx}
		//detail::nats_cmd_ack _cmd_ack;
		//_cmd_ack.name = g_current_command.name;
		//_cmd_ack.action = g_current_command.action;
		//_cmd_ack.result = true;
		//pimpl->ack_command(g_current_command_reply, _cmd_ack);
	}

	void nats_client::on_message()
	{
		int msg = 0;
	}

	void nats_client::on_payload_response()
	{
		// expect recv { name, size, result: true }
	}
}

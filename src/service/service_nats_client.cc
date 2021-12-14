#include "service_nats_client.h"
#include "service_utils.h"

#include <atomic>

freeze::atomic_sync g_message_signal{};
freeze::atomic_sync g_command_signal{};

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

		explicit _nats_msg(std::string const& sub, bool pak = false)
			:_sub{ sub }
		{
			// as heartbeat package
			if (pak)
			{
				_create(sub);
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

		bool set_cmd(nats_cmd const& cmd)
		{
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

		// caller should release this pointer
		char* set_payload(fs::path const file)
		{
			auto len = fs::file_size(file);
			char* data = new char[len] {};
			std::ifstream ifs(file, std::ios::binary);
			if (ifs.is_open())
			{
				ifs.read(data, len);
				ifs.close();
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
		bool _create(std::string const& reply = {})
		{
			_destroy();
			auto rep = reply.empty() ? nullptr : reply.c_str();
			natsMsg_Create(&_msg, _sub.c_str(), rep, nullptr, 0);
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
			auto ok = natsMsgHeader_Keys(_msg, &keys, &count) == NATS_OK;
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

	public:
		bool publish_message(std::string const& msg)
		{
			auto _m = _nats_msg{ message_channel.data(), msg };
			return natsConnection_PublishMsg(_nc, _m) == NATS_OK;
		}

		bool publish_command(nats_cmd const& cmd)
		{
			auto _m = _nats_msg{ command_channel.data() };
			_m.set_cmd(cmd);
			return natsConnection_PublishMsg(_nc, _m) == NATS_OK;
		}

		bool publish_payload(fs::path const& file)
		{
			_nats_msg m{ payload_channel.data() };
			auto pdata = m.set_payload(file);
			if (!pdata)
			{
				return false;
			}

			//natsConnection_Publish(_nc, subj, data, len);
			auto ok = natsConnection_PublishMsg(_nc, m) == NATS_OK;
			delete pdata;
			return ok;
		}

		bool subject_message()
		{
			_nats_sub _sub{nullptr};
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
					_nats_msg m{ msg };
					auto self = reinterpret_cast<_nats_connect*>(closure);
					auto cmd = m.get_cmd();
					self->on_command(cmd);
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

		void on_command(nats_cmd const& cmd)
		{
			auto name = cmd.name;
			auto action = cmd.action;
			auto _a = std::format("name={}, action={}\n"sv, name, action);
			OutputDebugStringA(_a.c_str());
			g_command_signal.notify();
		}

		// noop, unused.
		void on_payload(std::string const& msg)
		{
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
					if (!self)
					{
						break;
					}
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (self_ptr)
					{
						self_ptr->on_message();
					}
					if (!self_ptr->_msg_thread_running)
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
					if (!self)
					{
						break;
					}
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (self_ptr)
					{
						self_ptr->on_command();
					}
					if (!self_ptr->_cmd_thread_running)
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

	}

	void nats_client::notify_payload()
	{

	}

	void nats_client::on_command()
	{
		int cmd = 0;
	}

	void nats_client::on_message()
	{
		int msg = 0;
	}

	void nats_client::on_payload()
	{

	}
}

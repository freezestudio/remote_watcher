//
// service-nats-client
//
#include <atomic>

#include "service_watch_tree.h"
#include "service_nats_client.h"

#define MAX_IP4_SIZE 16
#define CONNECT_TIMEOUT 10000

// TODO: add ignore field.
// 1. current: only one command {name:"modify-folder", action:"path/to/folder"}
// 2. TODO: add command {name:"modify-ignores", action: "folder1, folder2, ..."}
freeze::detail::nats_cmd g_current_command;

// current:
// 1. {name:"list-disk"}
// 2. {name:"select-directory", folder:"path/to/directory"}
// 3. TODO: add select-files message {name: "select-files", folder: "path/to/directory"}
// 4. TODO: add watch-folder messgae {name: "watch-folder"}
// 5. {name: "tree-info", folder: "path/to/dir", ignores: [...ignores]}
// 6. {name: "sync-files"} publish all image file in folder.
std::string g_current_message;

/* extern */
std::string g_cmd_response = std::string{};

namespace freeze::detail
{
	static std::wstring nats_status(DWORD status)
	{

		switch (status)
		{
		case NATS_OK:
			return L"success"s;
		case NATS_ERR:
			return L"generic error"s;
		case NATS_PROTOCOL_ERROR:
			return L"protocol error"s;
		case NATS_IO_ERROR:
			return L"network communiacation I/O error"s;
		case NATS_LINE_TOO_LONG:
			return L"message too long";
		case NATS_CONNECTION_CLOSED:
			return L"connection closed"s;
		case NATS_NO_SERVER:
			return L"no server"s;
		case NATS_STALE_CONNECTION:
			return L"stale connection"s;
		case NATS_SECURE_CONNECTION_WANTED:
			return L"server TLS error"s;
		case NATS_SECURE_CONNECTION_REQUIRED:
			return L"TLS error"s;
		case NATS_CONNECTION_DISCONNECTED:
			return L"disconnected"s;
		case NATS_CONNECTION_AUTH_FAILED:
			return L"authentication failed"s;
		case NATS_NOT_PERMITTED:
			return L"action not permitted"s;
		case NATS_NOT_FOUND:
			return L"action not found"s;
		case NATS_ADDRESS_MISSING:
			return L"incorrect URL"s;
		case NATS_INVALID_SUBJECT:
			return L"invalid subject"s;
		case NATS_INVALID_ARG:
			return L"invalid argument"s;
		case NATS_INVALID_SUBSCRIPTION:
			return L"invalid subscription"s;
		case NATS_INVALID_TIMEOUT:
			return L"invalid timeout"s;
		case NATS_ILLEGAL_STATE:
			return L"illegal state"s;
		case NATS_SLOW_CONSUMER:
			return L"slow consumer"s;
		case NATS_MAX_PAYLOAD:
			return L"larger than max payload"s;
		case NATS_MAX_DELIVERED_MSGS:
			return L"max delivered messages"s;
		case NATS_INSUFFICIENT_BUFFER:
			return L"insufficient buffer"s;
		case NATS_NO_MEMORY:
			return L"insufficient memory"s;
		case NATS_SYS_ERROR:
			return L"SYS error"s;
		case NATS_TIMEOUT:
			return L"timeout"s;
		case NATS_FAILED_TO_INITIALIZE:
			return L"initialize failed"s;
		case NATS_NOT_INITIALIZED:
			return L"not initialized"s;
		case NATS_SSL_ERROR:
			return L"SSL error"s;
		case NATS_NO_SERVER_SUPPORT:
			return L"not support"s;
		case NATS_NOT_YET_CONNECTED:
			return L"not yet connected"s;
		case NATS_DRAINING:
			return L"draining"s;
		case NATS_INVALID_QUEUE_NAME:
			return L"Invalid queue name"s;
		case NATS_NO_RESPONDERS:
			return L"no responders"s;
		case NATS_MISMATCH:
			return L"consumer sequence mismatch"s;
		case NATS_MISSED_HEARTBEAT:
			return L"missed heartbeat"s;
		default:
			break;
		}
		return {};
	}
}

namespace freeze::detail
{
	struct _nats_options
	{
	public:
		_nats_options(std::nullptr_t = nullptr)
			: _options{nullptr}
		{
		}

		_nats_options(
			std::string const &url,
			std::string const &user,
			std::string const &pwd,
			std::string const &name) : _nats_options(name)
		{
			_url(url);
			_user_pwd(user, pwd);
		}

		_nats_options(
			std::string const &url,
			std::string const &token,
			std::string const &name) : _nats_options(name)
		{
			_url(url);
			_token(token);
		}

		explicit _nats_options(std::string const &name)
			: _options{nullptr}
		{
			_create();

			if (!name.empty())
			{
				_name(name);
			}
			_buffer_size(64 * 1024);
		}

		_nats_options(_nats_options const &) = default;
		_nats_options &operator=(_nats_options const &) = default;

		~_nats_options()
		{
			_destroy();
		}

		operator natsOptions *() const
		{
			return _options;
		}

	public:
		void reset()
		{
			_destroy();
		}

		_nats_options &reset(std::string const &url, std::string const &token, std::string const &name = {})
		{
			auto ok = true;
			//_destroy();

			if (!_options)
			{
				ok = _create();
				if (!ok)
				{
					return *this;
				}

				if (!name.empty())
				{
					ok = _name(name);
				}

				ok = _buffer_size(64 * 1024);
				ok = _url(url);
				ok = _token(token);
				ok = _timeout(CONNECT_TIMEOUT); // default 15s
				ok = _no_retry();
			}

			return *this;
		}

	public:
		bool set_url(std::string const &url)
		{
			return _url(url);
		}

		bool set_pass(std::string const &user, std::string const &pwd)
		{
			return _user_pwd(user, pwd);
		}

		bool set_token(std::string const &token)
		{
			return _token(token);
		}

		bool set_cnn_name(std::string const &name = {})
		{
			return _name(name);
		}

	private:
		bool _create()
		{
			auto status = natsOptions_Create(&_options);
			auto created = status == natsStatus::NATS_OK;
			if (!created)
			{
				DEBUG_STRING(L"_nats_options::_create(): error, status={}\n"sv, nats_status(status));
			}
			return created;
		}

		void _destroy()
		{
			if (_options)
			{
				natsOptions_Destroy(_options);
				_options = nullptr;
			}
		}

		bool _name(std::string const &name)
		{
			std::string _cnn_name = name;
			if (name.empty())
			{
				wchar_t name[MAX_COMPUTERNAME_LENGTH]{};
				DWORD name_length = MAX_COMPUTERNAME_LENGTH;
				auto ok = GetComputerName(name, &name_length);
				if (ok)
				{
					auto id = GetCurrentProcessId();
					auto _mbs_name = detail::to_utf8(name, name_length);
					_cnn_name = std::format("c-{}-{}"sv, _mbs_name, id);
				}
			}
			return natsOptions_SetName(_options, _cnn_name.c_str()) == natsStatus::NATS_OK;
		}

		bool _buffer_size(int size = 0)
		{
			// 0=default=32kb
			return natsOptions_SetIOBufSize(_options, size) == natsStatus::NATS_OK;
		}

		bool _user_pwd(std::string const &user, std::string const &pwd)
		{
			return natsOptions_SetUserInfo(_options, user.c_str(), pwd.c_str()) == natsStatus::NATS_OK;
		}

		bool _token(std::string const &token)
		{
			auto _token = token;
			if (token.empty())
			{
				_token = detail::read_token();
				if (_token.empty())
				{
					_token = "aH7g8Rxq0q"s;
					detail::save_token(detail::to_utf16(_token));
				}
			}

			auto status = natsOptions_SetToken(_options, _token.c_str());
			auto ok = status == natsStatus::NATS_OK;
			return ok;
		}

		bool _url(std::string const &url)
		{
			if (url.empty())
			{
				DEBUG_STRING(L"_nats_options::_url(): {}\n"sv, detail::to_utf16(NATS_DEFAULT_URL));
				return natsOptions_SetURL(_options, NATS_DEFAULT_URL) == natsStatus::NATS_OK;
			}

			std::string nats_url;
			if (!url.starts_with("nats://"))
			{
				nats_url = "nats://";
			}
			nats_url += url;

			auto _find = std::find_if(std::cbegin(url), std::cend(url), [](auto &&c)
									  { return c == ':'; });
			if (_find == url.end())
			{
				nats_url += ":4222";
			}
			DEBUG_STRING(L"_nats_options::_url(): {}\n"sv, detail::to_utf16(nats_url));
			return natsOptions_SetURL(_options, nats_url.c_str()) == natsStatus::NATS_OK;
		}

		bool _timeout(int64_t ms)
		{
			auto status = natsOptions_SetTimeout(_options, ms);
			return status == NATS_OK;
		}

		bool _no_retry()
		{
			auto status = natsOptions_SetRetryOnFailedConnect(_options, false, nullptr, nullptr);
			return status == NATS_OK;
		}

	private:
		natsOptions *_options = nullptr;
	};
}

namespace freeze::detail
{
	struct _nats_msg
	{
		explicit _nats_msg(natsMsg *m)
			: _msg{m}
		{
			if (m)
			{
				_sub = natsMsg_GetSubject(_msg);
			}
		}

		explicit _nats_msg(std::string const &sub, bool heartbeat = false)
			: _sub{sub}
		{
			// as heartbeat package
			if (heartbeat)
			{
				_create_empty(sub);
			}
		}

		_nats_msg(std::string const &sub, std::string const &m)
			: _sub{sub}
		{
			_create_text(m);
		}

		~_nats_msg()
		{
			_destroy();
		}

		operator natsMsg *() const
		{
			return _msg;
		}

		natsMsg **put()
		{
			return &_msg;
		}

		operator bool() const
		{
			return _msg != nullptr;
		}

		operator bool()
		{
			return _msg != nullptr;
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

		bool set_msg(std::string const &msg, std::string const &_type)
		{
			_destroy();
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, msg.c_str(), msg.size()) == NATS_OK;
			if (!ok)
			{
				return false;
			}
			const char *value = nullptr;
			auto status = natsMsgHeader_Get(_msg, "type", &value);
			if (status == NATS_OK)
			{
				if (!value || text_type != std::string_view(value))
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

		bool set_cmd(nats_cmd const &cmd)
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

		bool set_cmd_ack(std::string const &reply, nats_cmd_ack const &ack)
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

		/*
		 * @brief read image data to buffer.
		 * @return uint8_t buffer need count.
		 */
		uintmax_t set_blob(uint8_t *data, uintmax_t *len, fs::path const &folder, fs::path const &file)
		{
			auto full_path_file = (folder / file).lexically_normal();
			if (!fs::exists(full_path_file))
			{
				DEBUG_STRING(L"_nats_msg::set_blob() error: file {}, not exists.\n"sv, full_path_file.c_str());
				return 0;
			}
			if (nullptr == len)
			{
				DEBUG_STRING(L"_nats_msg::set_blob() error: file {}, len-ptr is nullptr.\n"sv, full_path_file.c_str());
				return 0;
			}
			if (*len == 0)
			{
				auto file_count = fs::file_size(full_path_file);
				*len = file_count;
				DEBUG_STRING(L"_nats_msg::set_blob() get file {}, file-size={}.\n"sv, full_path_file.c_str(), file_count);
				return file_count;
			}

			std::ifstream ifs;
			do
			{
				if (!fs::exists(full_path_file))
				{
					DEBUG_STRING(L"_nats_msg::set_blob() open file error: file {}, not exists.\n"sv, full_path_file.c_str());
					goto fail;
				}

				ifs.clear();
				ifs.open(full_path_file, std::ios::binary | std::ios::in);
				if (ifs.is_open())
				{
					auto &_self = ifs.read(reinterpret_cast<char *>(data), *len);
					auto read_count = ifs.gcount();
					auto ok = !!_self;
					if (!ok)
					{
						ifs.close();
						DEBUG_STRING(L"_nats_msg::set_blob() open file error: code={}.\n"sv, full_path_file.c_str());
						continue;
					}
					assert(*len == read_count);
					if ((*len) != read_count)
					{
						ifs.close();
						DEBUG_STRING(L"_nats_msg::set_blob() error: read file {} size.\n"sv, full_path_file.c_str());
						continue;
					}
					ifs.close();

					_destroy();
					ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, reinterpret_cast<char *>(data), *len) == NATS_OK;
					if (!ok)
					{
						DEBUG_STRING(L"_nats_msg::set_blob() error: create file {} message.\n"sv, full_path_file.c_str());
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
					ok = _add_header("size", std::to_string(*len));
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
					DEBUG_STRING(L"_nats_msg::set_blob(): file={}, count={}, success.\n"sv, full_path_file.c_str(), read_count);
					return read_count;
				}
				else
				{
					// goodbit=0x0, eofbit=0x1, failbit=0x2, badbit=0x4
					DEBUG_STRING(L"_nats_msg::set_blob() error: open file {} failure, rdstate={}.\n"sv, full_path_file.c_str(), ifs.rdstate());
				}
				Sleep(300);
			} while (!ifs.good());

		fail:
			DEBUG_STRING(L"_nats_msg::set_blob() error: maybe set file {} headers failure.\n"sv, full_path_file.c_str());
			return 0;
		}

		uintmax_t set_blob_ex(uint8_t *data, uintmax_t len, fs::path const &folder, fs::path const &file = {})
		{
			Sleep(300);
			auto _path_file = folder;
			if (!fs::is_empty(file))
			{
				_path_file /= file;
			}
			// TODO: use read_file_ex inteed.
			auto success = freeze::detail::read_file(_path_file);
			if (success)
			{
				return _set_blob_msg(data, len, file);
			}
			DEBUG_STRING(L"_nats_msg::set_blob_ex() error: make message failure.\n");
			return 0;
		}

	private:
		uintmax_t _set_blob_msg(uint8_t *data, uintmax_t len, fs::path const &file)
		{
			_destroy();
			auto ok = natsMsg_Create(&_msg, _sub.c_str(), nullptr, reinterpret_cast<char *>(data), len) == NATS_OK;
			if (!ok)
			{
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): create message error.\n");
				return 0;
			}
			ok = _clear_headers();
			if (!ok)
			{
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): clear header error.\n");
				return 0;
			}
			ok = _add_header("type", "data");
			if (!ok)
			{
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): add header[type] error.\n");
				return 0;
			}
			std::wstring filename = file.c_str();
			ok = _add_header("name", detail::to_utf8(filename.c_str(), filename.size()));
			if (!ok)
			{
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): add header[name] error.\n");
				return 0;
			}
			ok = _add_header("size", std::to_string(len));
			if (!ok)
			{
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): add header[size] error.\n");
				return 0;
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
				DEBUG_STRING(L"_nats_msg::_set_blob_msg(): add header[mime] error.\n");
				return 0;
			}
			return len;
		}

	private:
		bool _auto_ack()
		{
			return natsMsg_Ack(_msg, nullptr) == NATS_OK;
		}

		bool _create_empty(std::string const &reply = {})
		{
			_destroy();
			auto rep = reply.empty() ? nullptr : reply.c_str();
			return natsMsg_Create(&_msg, _sub.c_str(), rep, nullptr, 0) == NATS_OK;
		}

		bool _create_text(std::string const &m)
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

		std::string _create_reply_text(std::string const &reply, std::string const &m)
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

		bool _set_header(std::string key, std::string const &value)
		{
			return natsMsgHeader_Set(_msg, key.c_str(), value.c_str()) == NATS_OK;
		}

		bool _add_header(std::string key, std::string const &value)
		{
			return natsMsgHeader_Add(_msg, key.c_str(), value.c_str()) == NATS_OK;
		}

		bool _clear_headers()
		{
			const char **keys = nullptr;
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

			free((void *)keys);
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
		natsMsg *_msg = nullptr;
	};
}

namespace freeze::detail
{
	struct _nats_sub
	{
		explicit _nats_sub(natsSubscription *sub)
			: _sub{sub}
		{
		}

		natsSubscription **put()
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
			natsMsg *m;
			auto ok = natsSubscription_NextMsg(&m, _sub, timeout) == NATS_OK;
			if (ok)
			{
				return _nats_msg{m};
			}
			return _nats_msg{static_cast<natsMsg *>(nullptr)};
		}

	private:
		bool _destroy()
		{
			natsSubscription_Destroy(_sub);
		}

	private:
		natsSubscription *_sub;
	};
}

namespace freeze::detail
{
	struct _nats_connect
	{
		// TODO: make default name = {nats-pcname-pid}
	public:
		_nats_connect(/*std::nullopt_t*/) noexcept
			: _opts{}, _nc{nullptr}
		{
		}

		_nats_connect(
			std::string const &url,
			std::string const &user,
			std::string const &pwd,
			std::string const &name = {}) noexcept
			: _opts(url, user, pwd, name)
		{
			_connect();
		}

		_nats_connect(
			std::string const &url,
			std::string const &token,
			std::string const &name = {}) noexcept
			: _opts(url, token, name)
		{
			_connect();
		}

		~_nats_connect() noexcept
		{
			_destroy();
		}

		explicit operator bool() noexcept
		{
			return _nc != nullptr;
		}

		explicit operator bool() const noexcept
		{
			return _nc != nullptr;
		}

	public:
		void close()
		{
			_destroy();
		}

		void change_ip(uint32_t ip, std::string const &token /*= {}*/)
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

		_nats_connect &reset(std::string const &url, std::string const &token, std::string const &name = {})
		{
			_destroy();

			_opts.reset(url, token, name);
			auto ok = _connect();

			return *this;
		}

		bool ack_command(std::string const &reply, nats_cmd_ack const &ack)
		{
			auto _m = _nats_msg{command_channel.data()};
			auto ok = _m.set_cmd_ack(reply, ack);
			if (!ok)
			{
				DEBUG_STRING(L"_nats_connect::ack_command(): set ack command error.\n");
				return false;
			}
			auto status = natsConnection_PublishMsg(_nc, _m);
			ok = status == NATS_OK;
			if (!ok)
			{
				DEBUG_STRING(L"_nats_connect::ack_command(): error, status={}\n"sv, nats_status(status));
			}
			return ok;
		}

		DWORD maybe_heartbeat()
		{
			auto _m = _nats_msg{std::string(message_send_channel.data()), true};
			_nats_msg _reply_msg{static_cast<natsMsg *>(nullptr)};
			auto status = natsConnection_RequestMsg(_reply_msg.put(), _nc, _m, 3000);
			if (!_reply_msg)
			{
				return NATS_NO_RESPONDERS;
			}
			// NATS_TIMEOUT = 26
			return status;
		}

	public:
		bool publish_message(std::string const &msg, std::string const &_type)
		{
			// std::lock_guard<std::mutex> lock(_mutex);
			auto _m = _nats_msg{std::string(message_send_channel.data()), false};
			auto ok = _m.set_msg(msg, _type);
			if (ok)
			{
				auto status = natsConnection_PublishMsg(_nc, _m);
				ok = status == NATS_OK;
				if (ok)
				{
					DEBUG_STRING(L"_nats_connect::publish-message(): publish success.\n");
				}
				else
				{
					DEBUG_STRING(L"_nats_connect::publish-message(): failure, status={}.\n"sv, nats_status(status));
				}
			}
			else
			{
				DEBUG_STRING(L"_nats_connect::publish-message(): make message failure.\n");
			}
			return ok;
		}

		// unused.
		bool publish_command(nats_cmd const &cmd)
		{
			// std::lock_guard<std::mutex> lock(_mutex);
			auto _m = _nats_msg{std::string(command_channel.data()), false};
			_m.set_cmd(cmd);
			auto status = natsConnection_PublishMsg(_nc, _m);
			if (status != NATS_OK)
			{
				DEBUG_STRING(L"_nats_connect::publish_command: error, status={}\n"sv, nats_status(status));
			}
			return status == NATS_OK;
		}

		bool publish_payload(fs::path const &file_path, fs::path const &file_name)
		{
			DEBUG_STRING(L"_nats_connect::publish_payload(): send {}, {}\n"sv, file_path.c_str(), file_name.c_str());
			std::lock_guard<std::mutex> lock(_mutex);

			_nats_msg data_msg{payload_channel.data()};
			auto file_size = fs::file_size(file_path / file_name);
			if (file_size == 0)
			{
				DEBUG_STRING(L"_nats_connect::publish_payload() error: zero data.\n");
				return false;
			}
			std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(file_size);
			auto ret_count = data_msg.set_blob_ex(buffer.get(), file_size, file_path, file_name);
			if (ret_count == 0)
			{
				DEBUG_STRING(L"_nats_connect::publish_payload() error: data is null.\n");
				return false;
			}

			_nats_msg reply_msg{static_cast<natsMsg *>(nullptr)};
			auto status = natsConnection_RequestMsg(reply_msg.put(), _nc, data_msg, 60 * 1000);
			auto ok = status == NATS_OK;
			if (!ok)
			{
				// NATS_MAX_PAYLOAD = 21
				DEBUG_STRING(L"_nats_connect::publish_payload() request-msg error: status={}.\n"sv, nats_status(status));
				return ok;
			}

			try
			{
				// response={ name, size, result: true }
				auto _json_msg = reply_msg.get_msg();
				if (!_json_msg.empty())
				{
					using json = nlohmann::json;
					auto j = json::parse(_json_msg);
					std::string _mbs_name = j["name"];

					// test only
					auto _wcs_name = detail::to_utf16(_mbs_name);
					ok = fs::path{_wcs_name} == file_name;
					DEBUG_STRING(L"_nats_connect::publish_payload() reply message: {}, result={}.\n"sv, _wcs_name, ok);
				}
				else
				{
					DEBUG_STRING(L"_nats_connect::publish_payload() response message empty!\n");
					ok = false;
				}

				if (ok)
				{
					DEBUG_STRING(L"_nats_connect::publish_payload() response file: {}, success.\n"sv, file_name.c_str());
				}
				else
				{
					DEBUG_STRING(L"_nats_connect::publish_payload() error: response-msg failure.\n");
				}
			}
			catch (const std::exception &e)
			{
				// OutputDebugStringA(e.what());
				auto wcs = detail::to_utf16(std::string(e.what()));
				DEBUG_STRING(wcs.c_str());
			}

			return ok;
		}

		bool publish_file(fs::path const &file_path)
		{
			DEBUG_STRING(L"_nats_connect::publish_file(): send={}\n"sv, file_path.c_str());
			std::lock_guard<std::mutex> lock(_mutex);

			_nats_msg data_msg{synfile_send_channel.data()};
			auto file_size = fs::file_size(file_path);
			if (file_size == 0)
			{
				DEBUG_STRING(L"_nats_connect::publish_file() error: zero data.\n");
				return false;
			}
			std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(file_size);
			auto ret_count = data_msg.set_blob_ex(buffer.get(), file_size, file_path);
			if (ret_count == 0)
			{
				DEBUG_STRING(L"_nats_connect::publish_file() error: data is null.\n");
				return false;
			}

			_nats_msg reply_msg{static_cast<natsMsg *>(nullptr)};
			auto status = natsConnection_RequestMsg(reply_msg.put(), _nc, data_msg, 60 * 1000);
			auto ok = status == NATS_OK;
			if (!ok)
			{
				DEBUG_STRING(L"_nats_connect::publish_file() request-msg error: failure, status={}.\n"sv, nats_status(status));
			}

			// try test reply message
			// response = {name, size, result: true}

			return ok;
		}

		bool subject_recv_message()
		{
			if (!_nc)
			{
				DEBUG_STRING(L"_nats_connect::subject_recv_message: error [nc] is null.\n");
				return false;
			}

			_nats_sub _sub{static_cast<natsSubscription *>(nullptr)};
			auto status = natsConnection_Subscribe(
				_sub.put(), _nc, message_recv_channel.data(),
				[](natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
				{
					_nats_msg m{msg};
					auto self = reinterpret_cast<_nats_connect *>(closure);
					// TODO: maybe need read/write lock
					g_current_message = m.get_msg();
					if (!g_current_message.empty())
					{
						self->on_recv_message(g_current_message);
					}
				},
				this);
			return status == NATS_OK;
		}

		bool subject_syncfile_message()
		{
			if (!_nc)
			{
				DEBUG_STRING(L"_nats_connect::subject_syncfile_message: error [nc] is null.\n");
				return false;
			}

			_nats_sub _sub{static_cast<natsSubscription *>(nullptr)};
			auto status = natsConnection_Subscribe(
				_sub.put(), _nc, synfile_recv_channel.data(),
				[](natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
				{
					_nats_msg m{msg};
					auto self = reinterpret_cast<_nats_connect *>(closure);
					// TODO: maybe need read/write lock
					g_current_message = m.get_msg();
					if (!g_current_message.empty())
					{
						self->on_recv_message(g_current_message);
					}
				},
				this);
			return status == NATS_OK;
		}

		// TODO: use coroutine
		bool subject_command()
		{
			if (!_nc)
			{
				DEBUG_STRING(L"_nats_connect::subject_command: error [nc] is null.\n");
				return false;
			}

			_nats_sub _sub{static_cast<natsSubscription *>(nullptr)};
			// cb(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
			auto status = natsConnection_Subscribe(
				_sub.put(), _nc, command_channel.data(),
				[](natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
				{
					auto self = reinterpret_cast<_nats_connect *>(closure);
					if (!self)
					{
						DEBUG_STRING(L"_nats_connect::subject_command()::lambda error: self instance is null.\n");
						return;
					}
					if (!msg)
					{
						DEBUG_STRING(L"_nats_connect::subject_command(): message is null.\n");
						return;
					}

					_nats_msg m{static_cast<natsMsg *>(msg)};
					auto cmd = m.get_cmd();
					detail::nats_cmd_ack _ack;
					if (cmd.name.empty())
					{
						self->ack_command(m.get_reply(), _ack);
						return;
					}

					_ack.name = cmd.name;
					_ack.action = cmd.action;
					_ack.result = true;

					self->ack_command(m.get_reply(), _ack);
					self->on_command(cmd);
				},
				this);
			return status == NATS_OK;
		}

		std::string subject_response()
		{
			return {};
		}

	public:
		std::string remote_ip()
		{
			char ip[MAX_IP4_SIZE]{};
			auto status = natsConnection_GetConnectedServerId(_nc, ip, MAX_IP4_SIZE);
			if (status == NATS_OK)
			{
				return ip;
			}
			else
			{
				return {};
			}
		}

		bool connect(std::string const &url, std::string const &token, std::string const &name = {})
		{
			_destroy();

			_opts.reset(url, token, name);
			_opts.set_cnn_name();

			auto ok = _connect();
			// assert(ok);
			//  TODO: if !ok error
			if (!ok)
			{
				DEBUG_STRING(L"_nats_connect::conect failure.\n");
				_opts.reset();
			}
			return ok;
		}

		bool is_connected()
		{
			if (!_nc)
			{
				return false;
			}

			auto status = natsConnection_Status(_nc);
			return status == NATS_CONN_STATUS_CONNECTED;
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
		std::string conntect_status(int status = -1)
		{
			std::string strs;
			if (!_nc)
			{
				strs = "null-connect"s;
				return strs;
			}

			if (status < 0)
			{
				status = natsConnection_Status(_nc);
			}
			switch (status)
			{
			default:
				break;
			case NATS_CONN_STATUS_DISCONNECTED:
				strs = "disconnected"s;
				break;
			case NATS_CONN_STATUS_CONNECTING:
				strs = "connecting"s;
				break;
			case NATS_CONN_STATUS_CONNECTED:
				strs = "connected"s;
				break;
			case NATS_CONN_STATUS_CLOSED:
				strs = "closed"s;
				break;
			case NATS_CONN_STATUS_RECONNECTING:
				strs = "re-connecting"s;
				break;
			case NATS_CONN_STATUS_DRAINING_SUBS:
				strs = "draining-subscriptions"s;
				break;
			case NATS_CONN_STATUS_DRAINING_PUBS:
				strs = "draining-publishers"s;
				break;
			}
			return strs;
		}

		std::string error_message()
		{
			const char *error = nullptr;
			auto status = natsConnection_GetLastError(_nc, &error);
			if (status != NATS_OK)
			{
				return {};
			}

			std::string strerr = error;
			return strerr;
		}

	public:
		void on_recv_message(std::string const &msg)
		{
			if (msg.empty())
			{
				return;
			}

			auto wstr = detail::to_utf16(msg);
			DEBUG_STRING(L"_nats_connect::on_recv_message(): {}\n"sv, wstr.c_str());

			// msg = {"name": "sync-files"}
			if (msg.contains(MSG_SYNC_FILES)) // c++23
			{
				global_reason_signal.notify_reason(sync_reason_send_synfile);
				DEBUG_STRING(L"_nats_connect::on_recv_message(): need SleepThread run[sync_reason_send_synfile] notified!\n");
				// next, should call nats_client::notify_synfiles();
			}
			else
			{
				// notify global SleepThread run.
				global_reason_signal.notify_reason(sync_reason_recv_message);
				DEBUG_STRING(L"_nats_connect::on_recv_message(): need SleepThread run[sync_reason_recv_message] notified!\n");
				// next, should call nats_client::notify_message();
			}
		}

		void on_command(/*std::string const& reply,*/ nats_cmd const &cmd)
		{
			g_current_command.name = cmd.name;
			g_current_command.action = cmd.action;

			auto wcs_name = detail::to_utf16(cmd.name);
			auto wcs_action = detail::to_utf16(cmd.action);
			DEBUG_STRING(L"_nats_connect::on_command: name={}, action={}\n"sv, wcs_name, wcs_action);

			// notify global SleepThread run.
			global_reason_signal.notify_reason(sync_reason_recv_command);
			DEBUG_STRING(L"_nats_connect::on_command(): need SleepThread run[sync_reason_recv_command] notified!\n");
		}

	private:
		bool _connect()
		{
			// NATS_DEFAULT_URL; "nats://localhost:4222"
			//  if _opts==nullptr then _opts=NATS_DEFAULT_URL;
			auto status = natsConnection_Connect(&_nc, _opts);
			// NATS_NO_SERVER
			if (status != NATS_OK)
			{
				// NATS_NO_SERVER 6
				DEBUG_STRING(L"_nats_connect::_connect(): error, status={}.\n"sv, nats_status(status));
			}

			auto ok = status == NATS_OK;
			if (ok)
			{
				ok = _want_header_support();
			}

			return ok;
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
			if (_nc)
			{
				_disconnect();
				natsConnection_Destroy(_nc);
				if (_opts)
				{
					_opts.reset();
				}
				_nc = nullptr;
			}
			else
			{
				if (_opts)
				{
					_opts = nullptr;
				}
			}
		}

		bool _want_header_support()
		{
			// NATS_NO_SERVER_SUPPORT;
			auto status = natsConnection_HasHeaderSupport(_nc);
			if (status != NATS_OK)
			{
				DEBUG_STRING(L"_nats_connect::_want_header_support(): error, status={}.\n"sv, nats_status(status));
			}
			return status == NATS_OK;
		}

	private:
		std::mutex _mutex{};

	private:
		natsConnection *_nc = nullptr;
		_nats_options _opts;
	};
}

namespace freeze
{
	nats_client::nats_client()
		: pimpl{std::make_unique<detail::_nats_connect>()}, _message_signal{}, _command_signal{}, _payload_signal{}, _synfile_signal{}
	{
		DEBUG_STRING(L"nats_client::nats_client(): constructor.\n");
	}

	nats_client::~nats_client()
	{
		DEBUG_STRING(L"nats_client::~nats_client(): de-constructor.\n");
		close();
	}

	void nats_client::change_ip(DWORD ip, std::string const &token /*= {}*/)
	{
		pimpl->change_ip(ip, token);
	}

	bool nats_client::connect(DWORD ip, std::string const &token /* = {}*/)
	{
		std::unique_lock<std::mutex> lock(_mutex);

		auto url = std::string{};
		if (ip > 0)
		{
			url = detail::parse_ip_address(ip);
		}
		DEBUG_STRING(L"nats_client::connect(): will try connect to {}\n"sv,
					 url.empty() ? L"0.0.0.0" : detail::to_utf16(url));

		// pimpl.swap(detail::_nats_connect(url, token));
		auto _is_connected = pimpl->connect(url, token);
		if (!_is_connected)
		{
			DEBUG_STRING(L"nats_client::connect(): failure, nats client is null.\n");
			return false;
		}

		assert(pimpl != nullptr && (bool)(*pimpl.get()));
		_is_connected = pimpl->is_connected();
		if (_is_connected)
		{
			init_threads();
			DEBUG_STRING(L"nats_client::connect(): subject command and message.\n");
			// TODO: fix if return false
			pimpl->subject_command();
			pimpl->subject_recv_message();
			pimpl->subject_syncfile_message();
		}
		else
		{
			DEBUG_STRING(L"nats_client::connect(): connect to {} failure.\n"sv, detail::to_utf16(url));
		}
		return _is_connected;
	}

	void nats_client::close()
	{
		if (pimpl)
		{
			pimpl->close();
			stop_threads();
		}
	}

	bool nats_client::is_connected()
	{
		return pimpl->is_connected();
	}

	void nats_client::notify_message()
	{
		// notify message-thread resume.
		_message_signal.notify();
		DEBUG_STRING(L"nats_client::notify_message(): message-thread notified!\n");

		// should, next call nats_client::message_response().
		// should, SleepThread wait.
	}

	void nats_client::notify_command()
	{
		// notify command-thread resume.
		_command_signal.notify();
		DEBUG_STRING(L"nats_client::notify_command(): command-thread notified!\n");

		// should, next call nats_client::command_handle_result().
		// should, SleepThread wait.
	}

	void nats_client::notify_payload(fs::path const &root)
	{
		// notify payload-thread resume.
		_watch_path = root;
		_payload_signal.notify();
		DEBUG_STRING(L"nats_client::notify_payload(): payload-thread notified!\n");

		// should, next call nats_client::send_payload().
		// should, SleepThread wait.
	}

	void nats_client::notify_synfiles()
	{
		_synfile_signal.notify();
		DEBUG_STRING(L"nats_client::notify_synfiles(): synfile-thread notified!\n");

		// should, next call nats_client::sync_files().
		// should, SleepThread wait.
	}

	void nats_client::message_response()
	{
		// TODO: try...catch...
		// TODO: lock ...

		DEBUG_STRING(L"nats_client::message_response(): message={}\n"sv, detail::to_utf16(g_current_message));
		// std::unique_lock<std::mutex> lock(_mutex);
		if (g_current_message.empty())
		{
			DEBUG_STRING(L"nats_client::message_response(): message is empty!\n");
			return;
		}
		std::string _message = std::exchange(g_current_message, {});
		assert(g_current_message.empty());
		DEBUG_STRING(L"nats_client::message_response(): exchange={}\n"sv, detail::to_utf16(_message));
		// lock.unlock();

		// 1. {name:"list-disk"}
		// 2. {name:"select-directory", folder:"path/to/directory"}
		// 3. {name: "select-files", folder: "path/to/directory"}
		// 4. {name: "watch-folder"}
		// 5. {name: "tree-info", folder: "path/to/directory", ignores: [...]}

		auto _recv_msg = detail::parse_recv_message(_message);
		// assert(((!!(_recv_msg.name)) && (!(_recv_msg.name.empty()))));
		DEBUG_STRING(L"nats_client::message_response(): {} handling ...\n"sv, detail::to_utf16(_recv_msg.name));

		std::string _send_msg;
		if (_recv_msg.name == std::string(MSG_LIST_DISK))
		{
			DEBUG_STRING(L"nats_client::message_response(): notify-message[list-disk]: call get_harddisks_ex() ...\n");
			auto disk_names = detail::get_harddisks_ex();
			DEBUG_STRING(L"nats_client::message_response(): notify-message[list-disk]: disk count={}\n"sv, disk_names.size());
			_send_msg = detail::make_send_message_string(_recv_msg.name, disk_names);
		}
		else if (_recv_msg.name == std::string(MSG_LIST_DIR))
		{
			auto dir = detail::to_utf16(_recv_msg.folder);
			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-directory]: dir={}\n"sv, dir);

			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-directory]: call get_directories_without_subdir(dir) ...\n");
			auto folders = detail::get_directories_without_subdir(dir);
			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-directory]: folders-count={}\n"sv, folders.size());
			_send_msg = detail::make_send_message_string(_recv_msg.name, folders);
		}
		else if (_recv_msg.name == std::string(MSG_LIST_FILE)) // unused.
		{
			auto dir = detail::to_utf16(_recv_msg.folder);
			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-files]: dir={}\n"sv, dir);

			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-files]: call get_files_without_subdir(dir) ...\n");
			auto files = detail::get_files_without_subdir(fs::path{dir});
			DEBUG_STRING(L"nats_client::message_response(): notify-message[select-files]: files-count={}\n"sv, files.size());
			_send_msg = detail::make_send_message_string(_recv_msg.name, files);
		}
		else if (_recv_msg.name == std::string(MSG_FOLDER))
		{
			auto wcs_folder = detail::read_latest_folder();
			DEBUG_STRING(L"nats_client::message_response(): notify-message[watch-folder]: current watch folder: {}\n"sv, wcs_folder);

			std::vector<std::string> _one;
			if (!wcs_folder.empty())
			{
				// TODO: erase \u0000
				auto mbs_folder = detail::to_utf8(wcs_folder);
				_one.emplace_back(mbs_folder);
			}
			_send_msg = detail::make_send_message_string(_recv_msg.name, _one);
		}
		else if (_recv_msg.name == std::string(MSG_TREE_INFO))
		{
			_sync_path = fs::path{_recv_msg.folder};
			std::vector<fs::path>{}.swap(_sync_igonres);
			auto root = detail::to_utf16(_recv_msg.folder);
			std::vector<fs::path> ignores;
			if (_recv_msg.ignores.size() > 0)
			{
				for (auto const &item : _recv_msg.ignores)
				{
					_sync_igonres.push_back(item);
					ignores.emplace_back(detail::to_utf16(item));
				}
			}
			// TODO: refact _sync_path,ignore to _tree_info?
			auto tree_info = detail::get_dirtree_info(root, ignores);
			_send_msg = detail::make_send_message_string(_recv_msg.name, tree_info);
		}
		else
		{
			// empty.
			DEBUG_STRING(L"nats_client::message_response(): unknown message {}.\n"sv, detail::to_utf16(_recv_msg.name));
		}

		if (!_send_msg.empty())
		{
			pimpl->publish_message(_send_msg, json_type.data());
			DEBUG_STRING(L"nats_client::message_response(): response done.\n"sv);
		}
		else
		{
			DEBUG_STRING(L"nats_client::message_response(): send-msg is empty, not response!\n"sv);
		}
	}

	void nats_client::send_payload()
	{
		try
		{
			DEBUG_STRING(L"nats_client::send_payload(): from root path: {}.\n"sv, this->_watch_path.c_str());
			// std::unique_lock<std::mutex> lock(_mutex);
			auto watch_tree_ptr = watch_tree_instace(this->_watch_path);
			if (!watch_tree_ptr)
			{
				DEBUG_STRING(L"nats_client::send_payload(): watch-tree is null, payload not send.\n");
				return;
			}
			DEBUG_STRING(L"nats_client::send_payload(): read watch-tree files count={}.\n", watch_tree_ptr->current_count());

			auto files = watch_tree_ptr->get_all();
			DEBUG_STRING(L"nats_client::send_payload(): move-files count={}.\n"sv, files.size());

			watch_tree_ptr->clear();
			DEBUG_STRING(L"nats_client::send_payload(): expect watch-tree=0, actual watch-tree={}.\n"sv, watch_tree_ptr->current_count());
			// lock.unlock();

			DEBUG_STRING(L"nats_client::send_payload(): expect move-files>0: actual move-files={}.\n"sv, files.size());
			auto root_str = this->_watch_path.c_str();
			for (auto file : files)
			{
				DEBUG_STRING(L"nats_client::send_payload() will publish: {}\n"sv, file.c_str());
				pimpl->publish_payload(root_str, file);
			}
		}
		catch (const std::exception &e)
		{
			auto err_msg = e.what();
			DEBUG_STRING(L"nats_client::send_payload() error: {}"sv, detail::to_utf16(err_msg));
		}
	}

	void nats_client::sync_files()
	{
		try
		{
			DEBUG_STRING(L"nats_client::sync_files(): sync-path={}"sv, _sync_path.c_str());
			auto tree_paths = freeze::detail::get_dirtree_paths(_sync_path, _sync_igonres);
			DEBUG_STRING(L"nats_client::sync_files(): get_dirtree_paths size={}"sv, tree_paths.size());
			for (auto _path : tree_paths)
			{
				pimpl->publish_file(_path);
			}
		}
		catch (const std::exception &e)
		{
			auto err_msg = e.what();
			DEBUG_STRING(L"nats_client::sync_files() error: {}"sv, detail::to_utf16(err_msg));
		}
	}

	void nats_client::command_handle_result()
	{
		// std::unique_lock<std::mutex> lock(_mutex);
		if (g_current_command.name.empty())
		{
			DEBUG_STRING(L"nats_client::command_handle_result(): command is empty!\n");
			g_cmd_response = {};

			global_reason_signal.notify_reason(sync_reason_cmd__empty);
			DEBUG_STRING(L"nats_client::command_handle_result(): response empty, global_reason_signal notified.\n");
			return;
		}

		detail::nats_cmd cmd;
		cmd.name = g_current_command.name;
		cmd.action = g_current_command.action;

		g_current_command.name = {};
		g_current_command.action = {};
		// lock.unlock();

		std::string cmd_name = cmd.name;
		DEBUG_STRING(L"nats_client::command_handle_result(): command-name={}\n"sv, detail::to_utf16(cmd_name));
		auto sync_reason = sync_reason_cmd__error;
		if (cmd.name == CMD_FOLDER)
		{
			auto folder = detail::to_utf16(cmd.action);
			DEBUG_STRING(L"nats_client::command_handle_result(): will modify-folder={}!\n"sv, folder);
			if (!folder.empty())
			{
				if (fs::exists(fs::path{folder}))
				{
					// TODO: maybe need lock
					if (detail::save_latest_folder(folder))
					{
						DEBUG_STRING(L"nats_client::command_handle_result(): save latest modify-folder={}, success!\n"sv, folder);
						sync_reason = sync_reason_cmd_folder;
					}
				}
				else
				{
					DEBUG_STRING(L"nats_client::command_handle_result(): modify-folder={}, not exists!\n"sv, folder);
				}
			}
			else
			{
				DEBUG_STRING(L"nats_client::command_handle_result(): modify-folder={}, empty!\n"sv, folder);
			}
		}
		else if (cmd.name == CMD_IGNORE)
		{
			// folders = split(cmd.action, ','); // to_utf16(...)
			// save to regkey.
			sync_reason = sync_reason_cmd_igonre;
		}
		else
		{
			// empty
			DEBUG_STRING(L"nats_client::command_handle_result(): unknown command.\n");
		}

		g_cmd_response = cmd_name;
		DEBUG_STRING(L"nats_client::command_handle_result(): response command is: {}!\n"sv, detail::to_utf16(g_cmd_response));

		global_reason_signal.notify_reason(sync_reason);
		DEBUG_STRING(L"nats_client::command_handle_result(): global_reason_signal notified reason={}.\n"sv, sync_reason);
	}

	void nats_client::init_threads()
	{
		stop_threads();

		_msg_thread_running = true;
		_msg_thread = std::thread([](auto &&self)
								  {
				DEBUG_STRING(L"[nats_client] message-thread: starting ...\n");
				while (true)
				{
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						DEBUG_STRING(L"[nats_client] message-thread: self_ptr=null, stopping ...\n");
						break;
					}
					DEBUG_STRING(L"[nats_client] message-thread: waiting...\n");
					self_ptr->_message_signal.wait();
					DEBUG_STRING(L"[nats_client] message-thread: _message_signal wait ready.\n");
					if (!(self_ptr->_msg_thread_running))
					{
						DEBUG_STRING(L"[nats_client] message-thread: running=false, stopping ...\n");
						break;
					}
					self_ptr->message_response();
				}
				DEBUG_STRING(L"[nats_client] message-thread: stopped.\n"); },
								  this);

		_cmd_thread_running = true;
		_cmd_thread = std::thread([](auto &&self)
								  {
				DEBUG_STRING(L"[nats_client] command-thread: starting ...\n");
				while (true)
				{
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						DEBUG_STRING(L"[nats_client] command-thread: self_ptr=null, stopping ...\n");
						break;
					}
					DEBUG_STRING(L"[nats_client] command-thread: waiting...\n");
					self_ptr->_command_signal.wait();
					DEBUG_STRING(L"[nats_client] command-thread: _command_signal wait ready.\n");
					if (!(self_ptr->_cmd_thread_running))
					{
						DEBUG_STRING(L"[nats_client] command-thread: running=false, stopping ....\n");
						break;
					}
					self_ptr->command_handle_result();
				}
				DEBUG_STRING(L"[nats_client] command-thread: stopped.\n"); },
								  this);

		_pal_thread_running = true;
		_pal_thread = std::thread([](auto &&self)
								  {
				DEBUG_STRING(L"[nats_client] payload-thread: starting ...\n");
				while (true)
				{
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						DEBUG_STRING(L"[nats_client] payload-thread: self_ptr=null, stopping ...\n");
						break;
					}
					DEBUG_STRING(L"[nats_client] payload-thread: waiting...\n");
					self_ptr->_payload_signal.wait();
					DEBUG_STRING(L"[nats_client] payload-thread: _payload_signal wait ready.\n");
					if (!(self_ptr->_pal_thread_running))
					{
						DEBUG_STRING(L"[nats_client] payload-thread: running=false, stopping ....\n");
						break;
					}
					self_ptr->send_payload();
				}
				DEBUG_STRING(L"[nats_client] payload-thread: stopped.\n"); },
								  this);

		_syn_thread_running = true;
		_syn_thread = std::thread([](auto &&self)
								  {
				DEBUG_STRING(L"[nats_client] synfile-thread: starting ...\n");
				while (true)
				{
					auto self_ptr = reinterpret_cast<nats_client*>(self);
					if (!self_ptr)
					{
						DEBUG_STRING(L"[nats_client] synfile-thread: self_ptr=null, stopping ...\n");
						break;
					}
					DEBUG_STRING(L"[nats_client] synfile-thread: waiting...\n");
					self_ptr->_synfile_signal.wait();
					DEBUG_STRING(L"[nats_client] synfile-thread: _synfile_signal wait ready.\n");
					if (!(self_ptr->_syn_thread_running))
					{
						DEBUG_STRING(L"[nats_client] synfile-thread: running=false, stopping ....\n");
						break;
					}
					self_ptr->sync_files();
				}
				DEBUG_STRING(L"[nats_client] synfile-thread: stopped.\n"); },
								  this);
	}

	void nats_client::stop_threads()
	{
		DEBUG_STRING(L"nats_client::stop_threads(): stop all threads ...\n");
		if (_msg_thread_running)
		{
			_msg_thread_running = false;
			_message_signal.notify();
			// _message_signal.reset();
			if (_msg_thread.joinable())
			{
				_msg_thread.join();
			}
		}

		if (_cmd_thread_running)
		{
			_cmd_thread_running = false;
			_command_signal.notify();
			// _command_signal.reset();
			if (_cmd_thread.joinable())
			{
				_cmd_thread.join();
			}
		}

		if (_pal_thread_running)
		{
			_pal_thread_running = false;
			_payload_signal.notify();
			// _payload_signal.reset();
			if (_pal_thread.joinable())
			{
				_pal_thread.join();
			}
		}

		if (_syn_thread_running)
		{
			_syn_thread_running = false;
			_synfile_signal.notify();
			// _synfile_signal.reset();
			if (_syn_thread.joinable())
			{
				_syn_thread.join();
			}
		}
		DEBUG_STRING(L"nats_client::stop_threads(): all thread stopped.\n");
	}

	DWORD nats_client::maybe_heartbeat()
	{
		return pimpl->maybe_heartbeat();
	}
}

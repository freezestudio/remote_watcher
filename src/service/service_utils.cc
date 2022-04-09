#include "common_dep.h"
#include "service_utils.h"
// #include "service_utils_ex.h"

namespace freeze::detail
{
	std::string to_utf8(const wchar_t *wcs, int length)
	{
		// ccWideChar (characters), if string is null-terminated, can be set to -1
		// ccWideChar, if set to -1, function result including the terminating null character.
		// cbMultiByte (bytes), if set to 0, function returns the required buffer size.
		auto len = WideCharToMultiByte(CP_UTF8, 0, wcs, length, nullptr, 0, nullptr, nullptr);
		std::unique_ptr<char[]> pstr = std::make_unique<char[]>(len);
		len = WideCharToMultiByte(CP_UTF8, 0, wcs, length, pstr.get(), len, nullptr, nullptr);
		std::string str(pstr.get(), len);
		return str;
	}

	std::string to_utf8(std::wstring const &wcs)
	{
		return to_utf8(wcs.c_str(), (int)wcs.size());
	}

	std::wstring to_utf16(std::string const &mbs)
	{
		// cbMultiByte (bytes), if string is null-terminated, can be set to -1
		// cchWideChar (characters), if this value is 0, function return the required buffer size, including terminating null character.
		auto len = MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, nullptr, 0);
		auto pwstr = std::make_unique<wchar_t[]>(len);
		len = MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, pwstr.get(), len);
		std::wstring wcs(pwstr.get(), len - 1); // want erase null-terminated
		return wcs;
	}
}

namespace freeze::detail
{
	uint32_t make_ip_address(std::wstring const &ip)
	{
		if (ip.empty())
		{
			return EMPTY_IP;
		}

		// TODO: 1. check has '.' 3 times.
		// TODO: 2. check is string number.
		auto ip4len = ip.size();
		if (ip4len > 16ull)
		{
			return BAD_LEN_IP;
		}

		wchar_t cb[4][4]{};
		int index = 0;
		int sub_index = 0;
		for (auto c : ip)
		{
			if (c == L'.')
			{
				auto len = wcslen(cb[index]);
				if (len <= 0 || len > 3)
				{
					return BAD_IP;
				}
				index++;
				sub_index = 0;
				continue;
			}

			cb[index][sub_index++] = c;
			if (sub_index > 3)
			{
				return BAD_IP;
			}
		}

		int b[4]{};
		for (int i = 0; i < 4; ++i)
		{
			b[i] = _wtoi(cb[i]);
			if (b[i] < 0 || b[i] > 255)
			{
				return BAD_IP;
			}
		}
		// MAKEIPADDRESS
		return (((uint32_t)(b[0]) << 24) + ((uint32_t)(b[1]) << 16) + ((uint32_t)(b[2]) << 8) + ((uint32_t)(b[3])));
	}

	std::string parse_ip_address(uint32_t ip)
	{
		auto ip1 = (((ip) >> 24) & 0xff);
		auto ip2 = (((ip) >> 16) & 0xff);
		auto ip3 = (((ip) >> 8) & 0xff);
		auto ip4 = ((ip)&0xff);
		return std::format("{}.{}.{}.{}"sv, ip1, ip2, ip3, ip4).c_str();
	}

	bool check_ip_address(uint32_t ip)
	{
		return ((ip != EMPTY_IP) && (ip != BAD_LEN_IP) && (ip != BAD_IP));
	}
}

namespace freeze::detail
{
	fs::path to_normal(fs::path const &path)
	{
		return path.lexically_normal();
	}

	bool check_exists(fs::path const &path)
	{
		return !path.empty() && fs::exists(path);
	}

	bool normal_check_exists(fs::path const &path)
	{
		return check_exists(to_normal(path));
	}
}

namespace freeze::detail
{
	std::wstring to_str(response_type t)
	{
		std::wstring s;
		switch (t)
		{
		case response_type::result:
			s = L"result"s;
			break;
		case response_type::status:
			s = L"status"s;
			break;
		case response_type::overlapped:
			s = L"overlapped"s;
			break;
		default:
			break;
		}
		return s;
	}

	std::wstring to_str(uint32_t notify)
	{
		std::wstring s;
		switch (notify)
		{
		case FILE_ACTION_ADDED:
			[[fallthrough]];
		case FILE_ACTION_RENAMED_NEW_NAME:
			s = L"create"s;
			break;
		case FILE_ACTION_REMOVED:
			[[fallthrough]];
		case FILE_ACTION_RENAMED_OLD_NAME:
			s = L"remove"s;
			break;
		case FILE_ACTION_MODIFIED:
			s = L"modify"s;
			break;
		default:
			break;
		}
		return s;
	}
}

namespace freeze::detail
{
	static bool regkey_status(uint32_t status, LPCWSTR fn = nullptr)
	{
		if (ERROR_SUCCESS == status)
		{
			return true;
		}

		if (wchar_t *pBuffer = nullptr;
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM |		//
					FORMAT_MESSAGE_IGNORE_INSERTS | // dwFlags
					FORMAT_MESSAGE_ALLOCATE_BUFFER, //
				nullptr,							// lpSource
				status,								// dwMessageId
				0,									// dwLanguageId
				(wchar_t *)&pBuffer,				// lpBuffer
				0,									// nSize
				nullptr								// Arguments
				) > 0)
		{
			if (fn)
			{
				DEBUG_STRING(L"RegKey: {} Status Error: {}\n"sv, fn, pBuffer);
			}
			else
			{
				DEBUG_STRING(L"RegKey Status Error: {}\n"sv, pBuffer);
			}

			LocalFree(pBuffer);
		}
		return false;
	}

	bool save_ip(uint32_t ip)
	{
		freeze::reg_key regkey;
		auto status = regkey.Create(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return false;
		}
		status = regkey.SetDWORDValue(L"remote", ip);
		return regkey_status(status);
	}

	uint32_t read_ip()
	{
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return 0;
		}
		unsigned long value;
		status = regkey.QueryDWORDValue(L"remote", value);
		if (!regkey_status(status))
		{
			return 0;
		}
		return value;
	}

	bool save_latest_folder(std::wstring const &folder)
	{
		freeze::reg_key regkey;
		auto status = regkey.Create(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status, L"save_latest_folder"))
		{
			return false;
		}
		status = regkey.SetStringValue(L"latest", folder.c_str());
		Sleep(50);
		return regkey_status(status, L"save_latest_folder");
	}

	std::wstring read_latest_folder()
	{
		// TODO: how to lock threads.
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status, L"read_latest_folder"))
		{
			return {};
		}
		wchar_t value[MAX_PATH]{};
		unsigned long value_len = MAX_PATH;
		status = regkey.QueryStringValue(L"latest", value, &value_len);
		if (!regkey_status(status, L"read_latest_folder"))
		{
			return {};
		}
		// with null-terminated string.
		return std::wstring(value, value_len);
	}

	bool save_latest_ignores(std::vector<std::wstring> const &ignores)
	{
		freeze::reg_key regkey;
		auto status = regkey.Create(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return false;
		}
		auto total_size = 0;
		for (auto f : ignores)
		{
			total_size += f.size() + 1;
		}
		std::unique_ptr<wchar_t[]> value = std::make_unique<wchar_t[]>(total_size);
		auto value_pos = value.get();
		for (auto f : ignores)
		{
			auto _size = f.size() + 1;
			wmemcpy_s(value_pos, _size, f.c_str(), _size);
			value_pos += _size;
		}
		status = regkey.SetMultiStringValue(L"ignores", value.get());
		return regkey_status(status);
	}

	std::vector<std::wstring> read_latest_ignores()
	{
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return {};
		}
		std::unique_ptr<wchar_t[]> value = std::make_unique<wchar_t[]>(1024 * 1024);
		unsigned long value_len = 1024 * 1024;
		status = regkey.QueryMultiStringValue(L"ignores", value.get(), &value_len);
		if (!regkey_status(status))
		{
			return {};
		}
		// with null-terminated string.
		std::vector<std::wstring> ignores;
		auto value_pos = value.get();
		for (int i = 0; i < value_len;)
		{
			auto _size = wcslen(value_pos);
			auto v = std::wstring(value_pos, _size);
			ignores.emplace_back(v);
			i += _size + 1;
			value_pos += i;
		}
		return ignores;
	}

	bool save_token(std::wstring const &token)
	{
		freeze::reg_key regkey;
		auto status = regkey.Create(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return false;
		}
		status = regkey.SetStringValue(L"token", token.c_str());
		return regkey_status(status);
	}

	std::string read_token()
	{
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return {};
		}
		wchar_t value[11]{};
		unsigned long value_len = 11;
		status = regkey.QueryStringValue(L"token", value, &value_len);
		if (!regkey_status(status))
		{
			return {};
		}
		std::string mcs_value = to_utf8(value, (int)value_len);
		return mcs_value;
	}
}

namespace freeze::detail
{
	std::string _drive_type(UINT drive_type)
	{
		std::string type_name;
		switch (drive_type)
		{
		default:
			break;
		case DRIVE_UNKNOWN:
			type_name = "unknown"s;
			break;
		case DRIVE_NO_ROOT_DIR:
			type_name = "noroot"s;
			break;
		case DRIVE_REMOVABLE:
			type_name = "removable"s;
			break;
		case DRIVE_FIXED:
			type_name = "fixed"s;
			break;
		case DRIVE_REMOTE:
			type_name = "network"s;
			break;
		case DRIVE_CDROM:
			type_name = "cdrom"s;
			break;
		case DRIVE_RAMDISK:
			type_name = "ram"s;
			break;
		}
		return type_name;
	}

	std::vector<std::string> get_harddisks()
	{
		auto bitmask = GetLogicalDrives();
		auto bit_true_size = std::popcount(bitmask);
		constexpr auto disk_count = 'Z' + 1 - 'A';
		std::bitset<disk_count> bset(bitmask);

		std::vector<std::string> disks;
		auto idx = 0x41; // 'A'
		for (auto i = 0; i < disk_count; ++i)
		{
			if (bset[i])
			{
				disks.emplace_back(std::string{static_cast<char>(idx)});
			}
			idx++;
		}
		return disks;
	}

	std::vector<std::string> get_harddisks_ex()
	{
		auto bitmask = GetLogicalDrives();
		auto disk_count = std::popcount(bitmask);

		std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(1);
		auto need_len = GetLogicalDriveStrings(1, buffer.get());
		buffer = std::make_unique<wchar_t[]>(need_len + 1);
		need_len = GetLogicalDriveStrings(need_len + 1, buffer.get());

		std::vector<std::string> disk_names;
		auto name = buffer.get();
		for (auto i = 0; i < disk_count; ++i)
		{
			auto wcs = std::wstring(name);
			auto drive_type = GetDriveType(wcs.c_str());
			// filter: only fixed-drive
			if (drive_type == DRIVE_FIXED)
			{
				auto mbs = to_utf8(wcs.substr(0, 1));
				disk_names.push_back(mbs);
			}

			while (*name++ != L'\0')
				;
		}
		return disk_names;
	}

	std::vector<std::string> get_harddisk_names()
	{
		auto bitmask = GetLogicalDrives();
		auto disk_count = std::popcount(bitmask);

		std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(1);
		auto need_len = GetLogicalDriveStrings(1, buffer.get());
		buffer = std::make_unique<wchar_t[]>(need_len + 1);
		need_len = GetLogicalDriveStrings(need_len + 1, buffer.get());

		std::vector<std::string> disk_names;
		auto name = buffer.get();
		for (auto i = 0; i < disk_count; ++i)
		{
			auto wcs = std::wstring(name);
			auto mbs = to_utf8(wcs);
			disk_names.push_back(mbs);
			while (*name++ != L'\0')
				;
		}
		return disk_names;
	}

	std::vector<volume_type> get_volume_names()
	{
		constexpr auto length = 50;
		wchar_t name[length]{};
		auto hv = FindFirstVolume(name, length);
		if (hv == INVALID_HANDLE_VALUE)
		{
			return {};
		}

		auto mbs_volume = to_utf8(name, (int)wcslen(name));

		wchar_t dosname[length]{};
		std::vector<volume_type> volume_type_names;

		auto error = ERROR_SUCCESS;
		do
		{
			// get drive type
			auto drive_type = GetDriveType(name);

			// get dos name
			auto name_length = wcslen(name);
			name[name_length - 1] = L'\0';
			auto cch = QueryDosDevice(&name[4], dosname, length);
			auto mbs_device = to_utf8(dosname, (int)cch);
			wmemset(dosname, 0, length);

			// make tuple(volumename, dosname, drivetype)
			auto _tuple = std::make_tuple(mbs_volume, mbs_device, _drive_type(drive_type));
			volume_type_names.emplace_back(_tuple);

			// get next volume name
			wmemset(name, 0, length);
			auto ok = FindNextVolume(hv, name, length);
			if (!ok)
			{
				error = GetLastError();
				break;
			}
			mbs_volume = to_utf8(name, (int)name_length);
		} while (true);
		FindVolumeClose(hv);
		if (error == ERROR_NO_MORE_FILES)
		{
			hv = INVALID_HANDLE_VALUE;
		}
		return volume_type_names;
	}

	std::vector<std::string> get_directories_without_subdir(fs::path const &root)
	{
		std::vector<std::string> folders;
		if (root.empty() || !fs::exists(root))
		{
			return folders;
		}

		fs::directory_iterator iter{root};
		for (auto const &p : iter)
		{
			if (p.is_directory())
			{
				auto _folder = p.path().lexically_normal();
				auto mbs_folder = to_utf8(_folder.c_str());
				folders.emplace_back(mbs_folder);
			}
		}
		return folders;
	}

	std::vector<std::string> get_files_without_subdir(fs::path const &root)
	{
		std::vector<std::string> folders;
		if (root.empty() || !fs::exists(root))
		{
			return folders;
		}

		fs::directory_iterator iter{root};
		for (auto const &p : iter)
		{
			if (p.is_regular_file())
			{
				auto _folder = p.path().lexically_normal();
				auto mbs_folder = to_utf8(_folder.c_str());
				folders.emplace_back(mbs_folder);
			}
		}
		return folders;
	}

	std::vector<tree_information> get_dirtree_info(fs::path const &root, std::vector<fs::path> const &ignores)
	{
		std::vector<tree_information> tree;
		if (root.empty() || !fs::exists(root))
		{
			return tree;
		}

		auto is_include = [](auto vec, auto p)
		{
			for (auto i : vec)
			{
				if (i == p)
				{
					return true;
				}
			}
			return false;
		};

		auto equals = [](auto path_like)
		{
			std::regex suf("\\.(tif{1,2}|dcm)$", std::regex::ECMAScript | std::regex::icase);
			return std::regex_search(path_like, suf);
		};

		fs::recursive_directory_iterator iter{root};
		for (auto const &it : iter)
		{
			auto _path = it.path();
			if (is_include(ignores, _path))
			{
				iter.disable_recursion_pending();
				continue;
			}

			auto mbs_path = to_utf8(_path.c_str());
			// std::cout << iter.depth() << ": path=" << mbs_path << std::endl;

			if (it.is_regular_file())
			{
				if (equals(mbs_path))
				{
					auto path_name = to_utf8(_path.parent_path().c_str());
					auto file_name = to_utf8(_path.filename().c_str());
					auto file_size = it.file_size();
					auto info = tree_information{path_name, file_name, file_size};
					tree.emplace_back(info);
				}
			}
		}
		return tree;
	}

	std::vector<fs::path> get_dirtree_paths(fs::path const &root, std::vector<fs::path> const &ignores)
	{
		std::vector<fs::path> tree;
		if (root.empty() || !fs::exists(root))
		{
			return tree;
		}

		auto is_include = [](auto vec, auto p)
		{
			for (auto i : vec)
			{
				if (i == p)
				{
					return true;
				}
			}
			return false;
		};

		auto equals = [](auto path_like)
		{
			std::regex suf("\\.(tif{1,2}|dcm)$", std::regex::ECMAScript | std::regex::icase);
			return std::regex_search(path_like, suf);
		};

		fs::recursive_directory_iterator iter{root};
		for (auto const &it : iter)
		{
			auto _path = it.path();
			if (is_include(ignores, _path))
			{
				iter.disable_recursion_pending();
			}
			else
			{
				tree.emplace_back(_path);
			}
		}
		return tree;
	}
}

namespace freeze::detail
{
	bool read_file(fs::path const &file, uintmax_t size, uint8_t *data)
	{
		std::ifstream ifs;
		if (!fs::exists(file))
		{
			DEBUG_STRING(L"read_file() open file error: file {}, not exists.\n"sv, file.c_str());
			return false;
		}

		ifs.clear();
		ifs.open(file, std::ios::binary | std::ios::in);
		if (ifs.is_open())
		{
			auto &_self = ifs.read(reinterpret_cast<char *>(data), size);
			auto read_count = ifs.gcount();
			auto ok = !!_self;
			if (!ok)
			{
				ifs.close();
				DEBUG_STRING(L"read_file() open file error: code={}.\n"sv, ifs.rdstate());
				return false;
			}
			return true;
		}
		DEBUG_STRING(L"read_file() open file error: not opened, code={}.\n"sv, ifs.rdstate());
		return false;
	}

	bool read_file_ex(fs::path const &file, uintmax_t size, uint8_t *data)
	{
		// TODO: use asynchronous I/O
		auto file_handle = CreateFile(
			file.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, // FILE_FLAG_OVERLAPPED
			nullptr);
		if (INVALID_HANDLE_VALUE == file_handle)
		{
			auto err = GetLastError();
			DEBUG_STRING(L"read_file_ex: {}, CreateFile error={}\n"sv, file.c_str(), err);
			return false;
		}
		// TODO: use OVERLAPPED, or ReadFileEx
		DWORD number_of_bytes_read = 0;
		auto success = ReadFile(file_handle, data, size, &number_of_bytes_read, nullptr);
		if (!success)
		{
			auto err = GetLastError();
			DEBUG_STRING(L"read_file_ex: {}, ReadFile error={}\n"sv, file.c_str(), err);
		}
		CloseHandle(file_handle);
		return !!success;
	}
}

namespace freeze
{
	regkey_semaphore::regkey_semaphore()
	{
	}

	void regkey_semaphore::wait()
	{
		reset();
	}

	void regkey_semaphore::notify()
	{
		_singal.release();
	}

	void regkey_semaphore::reset()
	{
		_singal.acquire();
	}
}

namespace freeze
{
	locked_regkey::locked_regkey()
	{
	}

	locked_regkey::~locked_regkey()
	{
		_running = false;
		_semaphore.notify();
	}

	// locked_regkey &locked_regkey::instance()
	// {
	// 	static locked_regkey _regkey{};
	// 	return _regkey;
	// }

	void locked_regkey::save_latest_folder(std::wstring const &folder)
	{
		this->_folder = folder;
		_semaphore.notify();
		Sleep(50);
		_semaphore.wait();
	}

	std::wstring locked_regkey::read_latest_folder()
	{
		_semaphore.notify();
		Sleep(50);
		_semaphore.wait();
		return this->_folder;
	}

	void locked_regkey::thread_work()
	{
		while (true)
		{
			_semaphore.wait();
			if (!_running)
			{
				break;
			}

			if (_save)
			{
				detail::save_latest_folder(_folder);
			}
			else
			{
				_folder = detail::read_latest_folder();
			}
			_semaphore.notify();
		}
	}

	void locked_regkey::thread_loop()
	{
		_thread = std::jthread([this]
							   {
			if(this)
			{
				this->_running = true;
				this->thread_work();
			} });
	}
}

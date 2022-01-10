#include "common_dep.h"
#include "service_utils.h"
#include "service_utils_ex.h"

namespace freeze::detail
{
	std::string to_utf8(const wchar_t* wcs, int length)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0, wcs, length, nullptr, 0, nullptr, nullptr);
		std::unique_ptr<char[]> pstr = std::make_unique<char[]>(len + 1);
		WideCharToMultiByte(CP_UTF8, 0, wcs, length, pstr.get(), len, nullptr, nullptr);
		std::string str(pstr.get(), len);
		return str;
	}

	std::string to_utf8(std::wstring const& wcs)
	{
		return to_utf8(wcs.c_str(), (int)wcs.size());
	}

	std::wstring to_utf16(std::string const& mbs)
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, nullptr, 0);
		auto pwstr = std::make_unique<wchar_t[]>(len + 1);
		MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, pwstr.get(), len);
		std::wstring wcs(pwstr.get(), len);
		return wcs;
	}
}
namespace freeze::detail
{
	uint32_t make_ip_address(std::wstring const& ip)
	{
		if (ip.empty())
		{
			return 0;
		}
		auto ip4len = ip.size();
		if (ip4len > 16)
		{
			return 0;
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
					return 0;
				}
				index++;
				sub_index = 0;
				continue;
			}

			cb[index][sub_index++] = c;
			if (sub_index > 3)
			{
				return 0;
			}
		}

		int b[4]{};
		for (int i = 0; i < 4; ++i)
		{
			b[i] = _wtoi(cb[i]);
			if (b[i] < 0 || b[i]>255)
			{
				return 0;
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
		auto ip4 = ((ip) & 0xff);
		return std::format("{}.{}.{}.{}"sv, ip1, ip2, ip3, ip4).c_str();
	}
}

namespace freeze::detail
{
	fs::path to_normal(fs::path const& path)
	{
		return path.lexically_normal();
	}

	bool check_exists(fs::path const& path)
	{
		return !path.empty() && fs::exists(path);
	}

	bool normal_check_exists(fs::path const& path)
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
		case response_type::result: s = L"result"s; break;
		case response_type::status: s = L"status"s; break;
		case response_type::overlapped: s = L"overlapped"s; break;
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
		case FILE_ACTION_ADDED: [[fallthrough]];
		case FILE_ACTION_RENAMED_NEW_NAME:s = L"create"s; break;
		case FILE_ACTION_REMOVED: [[fallthrough]];
		case FILE_ACTION_RENAMED_OLD_NAME:s = L"remove"s; break;
		case FILE_ACTION_MODIFIED:s = L"modify"s; break;
		default:
			break;
		}
		return s;
	}
}

namespace freeze::detail
{
	static bool regkey_status(uint32_t status)
	{
		if (ERROR_SUCCESS == status)
		{
			return true;
		}

		if (wchar_t* pBuffer = nullptr;
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM |    //
				FORMAT_MESSAGE_IGNORE_INSERTS | // dwFlags
				FORMAT_MESSAGE_ALLOCATE_BUFFER, //
				nullptr,                        // lpSource
				status,                         // dwMessageId
				0,                              // dwLanguageId
				(wchar_t*)&pBuffer,               // lpBuffer
				0,                              // nSize
				nullptr                         // Arguments
			) > 0)
		{
			DEBUG_STRING(L"RegKey Status Error: {}\n"sv, pBuffer);
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

	bool save_latest_folder(std::wstring const& folder)
	{
		freeze::reg_key regkey;
		auto status = regkey.Create(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return false;
		}
		status = regkey.SetStringValue(L"latest", folder.c_str());
		return regkey_status(status);
	}

	std::wstring read_latest_folder()
	{
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return {};
		}
		wchar_t value[MAX_PATH]{};
		unsigned long value_len = MAX_PATH;
		status = regkey.QueryStringValue(L"latest", value, &value_len);
		if (!regkey_status(status))
		{
			return {};
		}
		// with null-terminated string.
		return std::wstring(value, value_len);
	}

	bool save_latest_ignores(std::vector<std::wstring> const& ignores)
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

	bool save_token(std::wstring const& token)
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
				disks.emplace_back(std::string{ static_cast<char>(idx) });
			}
			idx++;
		}
		return disks;
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

	std::string _drive_type(UINT dt)
	{
		std::string type_name;
		switch (dt)
		{
		default:
			break;
		case DRIVE_UNKNOWN:type_name = "unknown"s; break;
		case DRIVE_NO_ROOT_DIR:type_name = "noroot"s; break;
		case DRIVE_REMOVABLE:type_name = "removable"s; break;
		case DRIVE_FIXED:type_name = "fixed"s; break;
		case DRIVE_REMOTE:type_name = "network"s; break;
		case DRIVE_CDROM:type_name = "cdrom"s; break;
		case DRIVE_RAMDISK:type_name = "ram"s; break;
		}
		return type_name;
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

	std::vector<std::string> get_directories_without_subdir(fs::path const& root)
	{
		std::vector<std::string> folders;
		if (root.empty() || !fs::exists(root))
		{
			return folders;
		}

		fs::directory_iterator iter{ root };
		for (auto const& p : iter)
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


	std::vector<std::string> get_files_without_subdir(fs::path const& root)
	{
		std::vector<std::string> folders;
		if (root.empty() || !fs::exists(root))
		{
			return folders;
		}

		fs::directory_iterator iter{ root };
		for (auto const& p : iter)
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
}


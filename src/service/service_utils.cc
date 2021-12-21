#include "common_dep.h"
#include "service_utils.h"
#include "service_utils_ex.h"

namespace freeze::detail
{
	std::string to_utf8(const wchar_t* wcs, int length)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0, wcs, length, nullptr, 0, nullptr, nullptr);
		char* pstr = new char[len + 1]{};
		WideCharToMultiByte(CP_UTF8, 0, wcs, length, pstr, len, nullptr, nullptr);
		std::string str(pstr);
		delete[] pstr;
		return str;
	}

	std::string to_utf8(std::wstring const& wstr)
	{
		return to_utf8(wstr.c_str(), wstr.size());
	}

	std::wstring to_utf16(std::string const& mbs)
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, nullptr, 0);
		wchar_t* pwstr = new wchar_t[len + 1]{};
		MultiByteToWideChar(CP_UTF8, 0, mbs.c_str(), -1, pwstr, len);
		std::wstring res(pwstr);
		delete[] pwstr;
		return res;
	}
}
namespace freeze::detail
{
	DWORD make_ip_address(std::wstring const& ip)
	{
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
		return (((DWORD)(b[0]) << 24) + ((DWORD)(b[1]) << 16) + ((DWORD)(b[2]) << 8) + ((DWORD)(b[3])));
	}

	std::string parse_ip_address(DWORD ip)
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

	std::wstring to_str(DWORD notify)
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
	static bool regkey_status(DWORD status)
	{
		if (ERROR_SUCCESS == status)
		{
			return true;
		}

		if (LPWSTR pBuffer = nullptr;
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM |    //
				FORMAT_MESSAGE_IGNORE_INSERTS | // dwFlags
				FORMAT_MESSAGE_ALLOCATE_BUFFER, //
				nullptr,                        // lpSource
				status,                         // dwMessageId
				0,                              // dwLanguageId
				(LPWSTR)&pBuffer,               // lpBuffer
				0,                              // nSize
				nullptr                         // Arguments
			) > 0)
		{
			DEBUG_STRING(L"Error: {}\n"sv, pBuffer);
			LocalFree(pBuffer);
		}
		return false;
	}

	bool save_ip(DWORD ip)
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

	DWORD read_ip()
	{
		freeze::reg_key regkey;
		auto status = regkey.Open(HKEY_CURRENT_USER, L"Software\\richgolden\\rgmsvc");
		if (!regkey_status(status))
		{
			return 0;
		}
		DWORD value;
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
		ULONG value_len = MAX_PATH;
		status = regkey.QueryStringValue(L"latest", value, &value_len);
		if (!regkey_status(status))
		{
			return {};
		}
		// with null-terminated string.
		return std::wstring(value, value_len);
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
		ULONG value_len = 11;
		status = regkey.QueryStringValue(L"token", value, &value_len);
		if (!regkey_status(status))
		{
			return {};
		}
		std::string mcs_value = to_utf8(value, value_len);
		return mcs_value;
	}
}

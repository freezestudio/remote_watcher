#include "common_dep.h"
#include "service_utils.h"

// template<typename... Args>
// void debug_output(std::wstring_view const& fmt, Args&&... args)
// {
//     auto msg = std::format(fmt, args...);
//     OutputDebugString(msg.data());
// }

namespace freeze::detail
{
	
	std::string to_utf8(const wchar_t* wcs, int length)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0, wcs, length, NULL, 0, NULL, NULL);
		char* pstr = new char[len + 1];
		WideCharToMultiByte(CP_UTF8, 0, wcs, length, pstr, len, NULL, NULL);
		pstr[len] = '\0';
		std::string str(pstr);
		delete[] pstr;
		return str;
	}

	std::string to_utf8(std::wstring const& wstr)
	{
		return to_utf8(wstr.c_str(), wstr.size());
	}

	std::wstring to_utf16(std::string const& ncs)
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0, ncs.c_str(), -1, NULL, 0);
		wchar_t* pwstr = new wchar_t[len];
		MultiByteToWideChar(CP_UTF8, 0, ncs.c_str(), -1, pwstr, len);
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
			b[i]=_wtoi(cb[i]);
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
			//case FILE_ACTION_ADDED:s = L"add"s; break;
			//case FILE_ACTION_REMOVED:s = L"remove"s; break;
			//case FILE_ACTION_MODIFIED:s = L"modify"s; break;
			//case FILE_ACTION_RENAMED_OLD_NAME:s = L"rename-old-name"s; break;
			//case FILE_ACTION_RENAMED_NEW_NAME:s = L"rename-new-name"s; break;
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

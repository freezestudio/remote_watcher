#include "dep.h"
#include "utils.h"

// template<typename... Args>
// void debug_output(std::wstring_view const& fmt, Args&&... args)
// {
//     auto msg = std::format(fmt, args...);
//     OutputDebugString(msg.data());
// }

namespace freeze::detail
{
	
	std::string to_utf8(const wchar_t* input, int length)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0, input, length, NULL, 0, NULL, NULL);
		char* pstr = new char[len + 1];
		WideCharToMultiByte(CP_UTF8, 0, input, length, pstr, len, NULL, NULL);
		pstr[len] = '\0';
		std::string str(pstr);
		delete[] pstr;
		return str;
	}

	std::wstring to_utf16(std::string const& input)
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
		wchar_t* pwstr = new wchar_t[len];
		MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, pwstr, len);
		std::wstring res(pwstr);
		delete[] pwstr;
		return res;
	}

}

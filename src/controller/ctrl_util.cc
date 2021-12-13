#include "common_dep.h"
#include "ctrl_utils.h"
#include "ctrl_win32_dep.h"

#include <cassert>

// out namespaces
using namespace std::literals;
namespace fs = std::filesystem;

std::vector<std::wstring> enum_win32_services(SC_HANDLE scm)
{
	std::vector<std::wstring> vec_services;
	BOOL ret = FALSE;
	// ENUM_SERVICE_STATUS_PROCESS;
	uint8_t* services = nullptr;
	DWORD bytes_count = 0;
	DWORD bytes_needlen = 0;
	DWORD service_count = 0;
	DWORD resume = 0;
	do
	{
		ret = ::EnumServicesStatusEx(
			scm,
			SC_ENUM_PROCESS_INFO,
			SERVICE_WIN32,
			SERVICE_STATE_ALL,
			(LPBYTE)services,
			bytes_count,
			&bytes_needlen,
			&service_count,
			&resume,
			nullptr);
		if (!ret)
		{
			//ERROR_MORE_DATA; 234L
			auto err = GetLastError();
			if (err == ERROR_MORE_DATA)
			{
				if (bytes_needlen > 0)
				{
					bytes_count = bytes_needlen;
					if (services)
					{
						delete[] services;
						services = nullptr;
					}
					services = new uint8_t[bytes_count]{};
				}
			}
			else
			{
				/* error code */
			}
		}
		else
		{
			auto ssp = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(services);
			if (ssp)
			{
				for (int i = 0; i < static_cast<int>(service_count); ++i)
				{
					vec_services.push_back(ssp[i].lpServiceName);
				}
			}
			if (services)
			{
				delete[] services;
				services = nullptr;
			}
		}

	} while (!ret);
	return vec_services;
}

bool service_exists(SC_HANDLE scm, std::wstring const& name, bool ignore_case/*  = true */)
{
	auto services = enum_win32_services(scm);
	auto _end = std::end(services);
	if (ignore_case)
	{
		auto finded = std::find_if(std::begin(services), _end, [&name](auto&& v)
			{
				std::wstring _lname;
				std::transform(std::cbegin(name), std::cend(name), std::back_inserter(_lname), ::tolower);
				std::wstring _lv;
				std::transform(std::cbegin(v), std::cend(v), std::back_inserter(_lv), ::tolower);
				return _lname == _lv;
			});
		return finded != _end;
	}
	auto finded = std::find(std::begin(services), std::end(services), name);
	return finded != _end;
}

//
// verify regkey operator status
//
bool _verify_status(LPCWSTR op, DWORD status)
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
		auto msg = std::format(L"@rg {} Error: {}\n"sv, op, pBuffer);
		OutputDebugString(msg.data());
		LocalFree(pBuffer);
	}
	return false;
}

#ifndef SERVICE_PATH
bool append_host_value(
	LPCWSTR key/*  = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost" */,
	LPCWSTR subkey/*  = L"LocalService" */,
	LPCWSTR value/*  = SERVICE_NAME */)
{
	// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost
	//   LocalServer REG_MULTI_SZ  ...

	ATL::CRegKey svcHost;
	auto status = svcHost.Open(HKEY_LOCAL_MACHINE, key);
	if (!_verify_status(L"Open Key", status))
	{
		return false;
	}

	wchar_t exist_value[MAX_REGVALUE]{};
	ULONG len = MAX_REGVALUE;
	status = svcHost.QueryMultiStringValue(subkey, exist_value, &len);
	if (!_verify_status(L"Query Key", status))
	{
		return false;
	}

	// Append SERVICE_NAME to "LocalService"
	auto wcs_value = std::wstring(exist_value, len);
	auto pos = wcs_value.find(value);
	if (pos == std::wstring::npos)
	{
		auto _name_len = wcslen(value) + 1;
		WTL::SecureHelper::strcpy_x(exist_value + len - 1, _name_len, value);
		status = svcHost.SetMultiStringValue(subkey, exist_value);
		if (!_verify_status(L"Set RegValue", status))
		{
			return false;
		}
	}
	else
	{
		OutputDebugString(L"@rg Key-Value exists.\n");
	}
	OutputDebugString(L"@rg Append to Host Done.\n");
	return true;
}

bool remove_host_value(
	LPCWSTR key/*  = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost" */,
	LPCWSTR subkey/*  = L"LocalService" */,
	LPCWSTR value/*  = SERVICE_NAME */)
{
	// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost
	//   LocalServer REG_MULTI_SZ  ...

	ATL::CRegKey host;
	auto status = host.Open(HKEY_LOCAL_MACHINE, key);
	if (!_verify_status(L"Open Key for Remove", status))
	{
		return false;
	}

	wchar_t exist_value[MAX_REGVALUE]{};
	ULONG len = MAX_REGVALUE;
	status = host.QueryMultiStringValue(subkey, exist_value, &len);
	if (!_verify_status(L"Query Key for Remove", status))
	{
		return false;
	}

	auto wcs_value = std::wstring(exist_value, len);
	auto pos = wcs_value.find(value);
	if (pos == std::wstring::npos)
	{
		return true;
	}

	// Erase SERVICE_NAME from "LocalService"
	wcs_value.erase(pos);
	auto _new_str = wcs_value.data();
	auto _new_size = wcs_value.size();
	wmemset(exist_value, 0, MAX_REGVALUE);
	wmemcpy_s(exist_value, len, wcs_value.data(), wcs_value.size());
	status = host.SetMultiStringValue(subkey, exist_value);
	if (!_verify_status(L"Remove Host Value", status))
	{
		return false;
	}

	OutputDebugString(L"@rg Remove from Host Done.\n");
	return true;
}
#endif

bool add_svc_keyvalue(
	LPCWSTR key/*  = L"SYSTEM\\CurrentControlSet\\Services" */,
	LPCWSTR name/*  = SERVICE_NAME */
)
{
	// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services
	//   \rgmsvc                ...
	//   \rgmsvc\Parameters     ...

	ATL::CRegKey svc;
	auto maybe_create_svc = std::wstring(key) + L"\\" + name;
	// Create or Open key
	auto status = svc.Create(HKEY_LOCAL_MACHINE, maybe_create_svc.c_str());
	if (!_verify_status(L"Create or Open Svc Key", status))
	{
		return false;
	}

#ifdef SERVICE_PATH
	auto display_name = std::format(L"@%ProgramFiles%\\{}\\{}.exe,-101"sv, SERVICE_PATH, SERVICE_NAME);
#else
	// DisplayName REG_SZ        @%SystemRoot%\\System32\\rgmsvc.dll,-101
	auto display_name = std::format(L"@%SystemRoot%\\System32\\{}.dll,-101"sv, SERVICE_NAME);
#endif
	status = svc.SetStringValue(L"DisplayName", display_name.data());
	if (!_verify_status(L"Set Svc Value: DisplayName", status))
	{
		return false;
	}

#ifdef SERVICE_PATH
	auto description = std::format(L"@%ProgramFiles%\\{}\\{}.exe,-102"sv, SERVICE_PATH, SERVICE_NAME);
#else
	// Description REG_SZ        @%SystemRoot%\\System32\\rgmsvc.dll,-102
	auto description = std::format(L"@%SystemRoot%\\System32\\{}.dll,-102"sv, SERVICE_NAME);
#endif
	status = svc.SetStringValue(L"Description", description.data());
	if (!_verify_status(L"Set Svc Value: Description", status))
	{
		return false;
	}

#ifndef SERVICE_PATH
	// ImagePath   REG_EXPAND_SZ  %SystemRoot%\\System32\\svchost.exe -k LocalService
	// deprecated. use CreateService(...)
	//status = svc.SetStringValue(L"ImagePath", L"%SystemRoot%\\System32\\svchost.exe -k LocalService", REG_EXPAND_SZ);
	//if (!_verify_status(L"Set Svc Value: ImagePath", status))
	//{
	//	return false;
	//}

	// ObjectName  REG_SZ        NT AUTHORITY\\LocalService
	status = svc.SetStringValue(L"ObjectName", L"NT AUTHORITY\\LocalService");
	if (!_verify_status(L"Set Svc Value: ObjectName", status))
	{
		return false;
	}
	status = svc.SetDWORDValue(L"ServiceSidType", 1);
	if (!_verify_status(L"Set Svc Value: ServiceSidType", status))
	{
		return false;
	}
	// Start: SERVICE_START_PENDING; // 2
	// Type:  SERVICE_WIN32;         // 30
	// 
	// Start       REG_DWORD     2
	// Type        REG_DWORD     30
#endif

	// deprecated. use CreateService(...)
	//status = svc.SetDWORDValue(L"Start", SERVICE_START_PENDING);
	//if (!_verify_status(L"Set Svc Value: Start", status))
	//{
	//	return false;
	//}
	//status = svc.SetDWORDValue(L"Type", SERVICE_WIN32);
	//if (!_verify_status(L"Set Svc Value: Start", status))
	//{
	//	return false;
	//}

#ifndef SERVICE_PATH
	svc.Close();

	auto maybe_create_svc_params = maybe_create_svc + L"\\Parameters";
	status = svc.Create(HKEY_LOCAL_MACHINE, maybe_create_svc_params.c_str());
	if (!_verify_status(L"Create or Open Svc\\param Key", status))
	{
		return false;
	}

	// ServiceDll             REG_EXPAND_SZ %ProgramFiles%\\SERVICE_PATH\\SERVICE_NAME.dll
	// ServiceDll             REG_EXPAND_SZ %SystemRoot%\\System32\\SERVICE_NAME.dll
	// ServiceDllUnloadOnStop REG_DWORD     1
	// ServiceMain            REG_SZ        ServiceMain

	auto service_dll = std::format(L"%SystemRoot%\\System32\\{}.dll"sv, SERVICE_NAME);
	status = svc.SetStringValue(L"ServiceDll", service_dll.data(), REG_EXPAND_SZ);
	if (!_verify_status(L"Set Svc\\Parameters Value: ServiceDll", status))
	{
		return false;
	}
	status = svc.SetStringValue(L"ServiceMain", L"ServiceMain");
	if (!_verify_status(L"Set Svc\\Parameters Value: ServiceMain", status))
	{
		return false;
	}
	status = svc.SetDWORDValue(L"ServiceDllUnloadOnStop", 1);
	if (!_verify_status(L"Set Svc\\Parameters Value: ServiceDllUnloadOnStop", status))
	{
		return false;
	}
#endif

	OutputDebugString(L"@rg Add Svc Key-Values Done.\n");
	return true;
}

// for dll service, all force delete service.
bool remove_svc_keyvalue(
	LPCWSTR key/*  = L"SYSTEM\\CurrentControlSet\\Services" */,
	LPCWSTR name/*  = SERVICE_NAME */
)
{
	auto maybe_delete = std::wstring(key) + L"\\" + name;
	{
		ATL::CRegKey _svc;
		auto status = _svc.Open(HKEY_LOCAL_MACHINE, maybe_delete.c_str());
		if (ERROR_SUCCESS != status)
		{
			OutputDebugString(L"@rg RemoveKey: not exists.\n");
			return true;
		}
	}

	ATL::CRegKey svc{ HKEY_LOCAL_MACHINE };
	auto status = svc.RecurseDeleteKey(maybe_delete.c_str());
	auto ok = _verify_status(L"Delete Svc Keys", status);
	if (ok)
	{
		OutputDebugString(L"@rg Remove Svc Key Done.\n");
	}
	else
	{
		OutputDebugString(L"@rg Remove Svc Key Error.\n");
	}
	return ok;
}

bool decompress_blobs(void* blob, int len, const char* out_path)
{
	// 1. save tgz = path/to/temp/../blob.tgz
	// 2. tar(tgz, to out_path);

	auto _temp_path = fs::temp_directory_path();
#ifdef SERVICE_PATH
	_temp_path /= SERVICE_PATH;
	if (!fs::exists(_temp_path))
	{
		std::error_code ec;
		auto created = fs::create_directory(_temp_path, ec);
		if (!created || ec)
		{
			// error
			auto _msg = ec.message();
			OutputDebugStringA(_msg.c_str());
			return false;
		}
	}
#endif
	auto _temp_tgz = _temp_path / BLOB_NAME;
	auto _htmp = ::CreateFile(
		_temp_tgz.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (_htmp == INVALID_HANDLE_VALUE)
	{
		OutputDebugString(L"@rg before pick tgz, create temp file failure.\n");
		return false;
	}

	// blob data write to temp/../blob.tgz
	DWORD written;
	auto writted = ::WriteFile(_htmp, blob, len, &written, nullptr);
	// assert(len==written);
	CloseHandle(_htmp);
	if (!writted)
	{
		OutputDebugString(L"@rg write tgz to temp file failured.\n");
		return false;
	}

	char _tgz[MAX_PATH]{};

	// error? use to_utf8()
	auto _str_tgz = _temp_tgz.generic_string();

	//auto _generic_path = _temp_tgz.generic_wstring();
	//auto _str_tgz = to_utf8(_generic_path);

	strcpy(_tgz, _str_tgz.c_str());
	assert(strlen(_tgz) == _str_tgz.size());

	// decompress temp/../blob.tgz to install path
	auto ok = tar(_tgz, out_path) == 0;
	if (!ok)
	{
		OutputDebugString(L"@rg decompress tgz failure.\n");
	}

#ifdef SERVICE_PATH
	// remove temp path
	std::error_code ec;
	auto removed = fs::remove_all(_temp_tgz, ec);
	if (!removed || ec)
	{
		auto _msg = ec.message();
		OutputDebugString(L"@rg remove temp path to tgz file failure.\n");
		OutputDebugStringA(_msg.data());
		return false;
	}
#else
	// remove temp file
	std::error_code ec;
	auto removed = fs::remove(_temp_tgz, ec);
	if (!removed || ec)
	{
		auto _msg = ec.message();
		OutputDebugString(L"@rg remove temp tgz file failure.\n");
		OutputDebugStringA(_msg.data());
		return false;
	}
#endif
	return true;
}

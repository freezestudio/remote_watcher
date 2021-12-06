#include "dep.h"
#include "wdep.h"
#include "setup.h"
#include "assets.h"

#include <cassert>

#define BLOB_NAME L"blob.tgz"
// if defined, run %ProgramFiles%\SERVICE_PATH\*.exe service,
// else run %SystemRoot%\System32\*.dll service
#define SERVICE_PATH L"xMonit"

namespace fs = std::filesystem;
using namespace std::literals;

// deprecated.
static std::string to_utf8(std::wstring const& wstr)
{
	char _path[MAX_PATH]{};
	auto _len = ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), wstr.size(), _path, MAX_PATH, nullptr, nullptr);
	return std::string(_path, _len);
}

static std::vector<std::wstring> enum_win32_services(SC_HANDLE scm)
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
			ERROR_MORE_DATA;
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
			for (int i = 0; i < static_cast<int>(service_count); ++i)
			{
				vec_services.push_back(ssp[i].lpServiceName);
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

bool service_exists(SC_HANDLE scm, std::wstring const& name, bool ignore_case = true)
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

// Need >= Windows 8
// static void compress_blob()
// {
//
//     DECOMPRESSOR_HANDLE hdecomp;
//     auto _created = CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &hdecomp);
//     if(!_created)
//     {
//         auto _msg = std::format(L"Failed to create decompressor: {}", GetLastError());
//         OutputDebugString(_msg.data());
//         return;
//     }
//
//     PBYTE _buffer = nullptr;
//     size_t _buffer_size = 0;
//     auto _decompressed = Decompress(hdecomp, _data, _data_size, _buffer, _buffer_size, &_buffer_size);
//     if (!_decompressed)
//     {
//         auto _msg = std::format(L"Failed to decompress: {}", GetLastError());
//         OutputDebugString(_msg.data());
//         auto _err = GetLastError();
//         if(_err != ERROR_INSUFFICIENT_BUFFER)
//         {
//             return;
//         }
//         _buffer = new BYTE[_buffer_size];
//         if(!_buffer)
//         {
//             return;
//         }
//     }
//     _decompressed = Decompress(hdecomp, _data, _data_size, _buffer, _buffer_size, &_buffer_size);
//     if(!_decompressed)
//     {
//         auto _msg = std::format(L"Failed to decompress: {}", GetLastError());
//         OutputDebugString(_msg.data());
//         return;
//     } 
//     // copy all decompressed files to install path
//     // ...
//
//     delete [] _buffer;
//     CloseDecompressor(hdecomp);   
// }

static bool read_resource(void** data, int* len)
{

	auto _resource = WTL::CResource();
	auto _loaded = _resource.Load(MAKEINTRESOURCE(IDR_BLOB), MAKEINTRESOURCE(IDR_MAINFRAME));
	if (!_loaded)
	{
		auto _msg = std::format(L"@rg Failed to load resource: {}", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}
	*len = _resource.GetSize();
	*data = _resource.Lock();

	return true;
}


static bool _verify_status(LPCWSTR op, DWORD status)
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
static bool append_host_value(
	LPCWSTR key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
	LPCWSTR subkey = L"LocalService",
	LPCWSTR value = SERVICE_NAME)
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

static bool remove_host_value(
	LPCWSTR key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
	LPCWSTR subkey = L"LocalService",
	LPCWSTR value = SERVICE_NAME)
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

static bool add_svc_keyvalue(
	LPCWSTR key = L"SYSTEM\\CurrentControlSet\\Services",
	LPCWSTR name = SERVICE_NAME
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
static bool remove_svc_keyvalue(
	LPCWSTR key = L"SYSTEM\\CurrentControlSet\\Services",
	LPCWSTR name = SERVICE_NAME
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

static bool decompress_blobs(void* blob, int len, const char* out_path)
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

static bool set_description(SC_HANDLE sc)
{
#if !(defined(_UNICODE) || defined(UNICODE))
	return true;
#endif
	SERVICE_DESCRIPTION sdesc;
	auto _desc = std::format(L"@%ProgramFiles%\\{}\\{}.exe,-102"sv, SERVICE_PATH, SERVICE_NAME);
	sdesc.lpDescription = _desc.data();
	auto ok = ChangeServiceConfig2(sc, SERVICE_CONFIG_DESCRIPTION, &sdesc) ? true : false;
	return ok;
}

static bool query_status(SC_HANDLE sc, SERVICE_STATUS& service_status)
{
	BOOL ret = FALSE;
	uint8_t* buffer = nullptr;
	DWORD buffer_size = 0;
	DWORD bytes_needed = 0;
	do
	{
		ret = QueryServiceStatusEx(
			sc, SC_STATUS_PROCESS_INFO, buffer, buffer_size, &bytes_needed);
		if (!ret)
		{
			auto err = GetLastError();
			if (err == ERROR_INSUFFICIENT_BUFFER)
			{
				buffer_size = bytes_needed;
				if (buffer)
				{
					delete[] buffer;
					buffer = nullptr;
				}
				buffer = new uint8_t[buffer_size]{};
				ret = QueryServiceStatusEx(
					sc, SC_STATUS_PROCESS_INFO,
					buffer, buffer_size, &bytes_needed);
			}
			else
			{
				return false;
			}
		}
	} while (!ret);

	auto pstatus = reinterpret_cast<LPSERVICE_STATUS_PROCESS>(buffer);
	if (!pstatus)
	{
		return false;
	}

	service_status.dwCheckPoint = pstatus->dwCheckPoint;
	service_status.dwControlsAccepted = pstatus->dwControlsAccepted;
	service_status.dwCurrentState = pstatus->dwCurrentState;
	service_status.dwServiceSpecificExitCode = pstatus->dwServiceSpecificExitCode;
	service_status.dwServiceType = pstatus->dwServiceType;
	service_status.dwWaitHint = pstatus->dwWaitHint;
	service_status.dwWin32ExitCode = pstatus->dwWin32ExitCode;

	delete[] buffer;
	return true;
}

std::wstring _state(DWORD state)
{
	//SERVICE_STOPPED                        0x00000001
	//SERVICE_START_PENDING                  0x00000002
	//SERVICE_STOP_PENDING                   0x00000003
	//SERVICE_RUNNING                        0x00000004
	//SERVICE_CONTINUE_PENDING               0x00000005
	//SERVICE_PAUSE_PENDING                  0x00000006
	//SERVICE_PAUSED                         0x00000007

	std::wstring ret;
	switch (state)
	{
	default: break;
	case SERVICE_STOPPED: ret = L"stopped"; break;
	case SERVICE_START_PENDING: ret = L"start pending"; break;
	case SERVICE_STOP_PENDING: ret = L"stop pending"; break;
	case SERVICE_RUNNING: ret = L"running"; break;
	case SERVICE_CONTINUE_PENDING: ret = L"continue pending"; break;
	case SERVICE_PAUSE_PENDING: ret = L"pause pending"; break;
	case SERVICE_PAUSED: ret = L"paused"; break;
	}
	return ret;
}

//
// install service to %ProgramFiles% or %SystemRoot% dependence SERVICE_PATH defined.
//
bool install_service()
{
	wchar_t _path_buf[MAX_PATH]{};
#ifdef SERVICE_PATH
	auto _path = L"%ProgramFiles%\\" SERVICE_PATH;
#else
	auto _path = L"%SystemRoot%\\System32";
#endif
	ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
	auto _install_path = fs::path(_path_buf);

#ifdef SERVICE_PATH
	if (!fs::exists(_install_path))
	{
		std::error_code ec;
		auto created = fs::create_directories(_install_path, ec);
		if (!created || ec)
		{
			auto _msg = ec.message();
			OutputDebugStringA(_msg.data());
			return false;
		}
	}
#endif
	OutputDebugString(L"@rg Install Service: Select service path.\n");

	// decompress blob
	auto _resource = WTL::CResource();
	void* _data = nullptr;
	int _len = 0;
	if (!read_resource(&_data, &_len))
	{
		OutputDebugString(L"Read Service Resource Failure.\n");
		return false;
	}
	if (!decompress_blobs(_data, _len, _install_path.string().data()))
	{
		_resource.Release();
		OutputDebugString(L"@rg decompress blob failure.\n");
		return false;
	}
	_resource.Release();
	OutputDebugString(L"@rg Install Service: Expand service to path.\n");

	// open service control manager
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		OutputDebugString(L"@rg Install Service: OpenSCManager Failure.\n");
		return false;
	}

	// create service ...

	//wchar_t _display_name[255]{};
	//LoadString(nullptr, IDS_DISPLAY, _display_name, 255);

#ifdef SERVICE_PATH
	wchar_t _binary_path[MAX_PATH]{};
	auto _b_path = std::format(L"%ProgramFiles%\\{}\\{}.exe"sv, SERVICE_PATH, SERVICE_NAME);
	wcscpy_s(_binary_path, _b_path.data());
	auto _service_type = SERVICE_WIN32_OWN_PROCESS; // or SERVICE_WIN32;
	wchar_t _display_name[100]{};
	auto _d_name = std::format(L"@%ProgramFiles%\\{}\\{}.exe,-101"sv, SERVICE_PATH, SERVICE_NAME);
	wcscpy_s(_display_name, _d_name.data());
#else
	auto _binary_path = L"%SystemRoot%\\System32\\svchost.exe -k LocalService";
	auto _service_type = SERVICE_USER_SHARE_PROCESS;
	auto _display_name = nullptr;
#endif
	auto hsc = CreateService(
		hscm,
		SERVICE_NAME,
		_display_name,
		SERVICE_ALL_ACCESS,
		_service_type,
		SERVICE_DEMAND_START, // change to SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		_binary_path,
		nullptr, nullptr, nullptr, nullptr, nullptr
	);
	if (!hsc)
	{
		//ERROR_INVALID_PARAMETER; // 87
		auto _err = GetLastError();
		auto _msg = std::format(
			L"@rg Install Service: CreateService Failure: {}. name[{}], bin[{}]\n"sv,
			_err, SERVICE_NAME, _binary_path);
		OutputDebugString(_msg.data());
		CloseServiceHandle(hscm);
		return false;
	}

#ifdef SERVICE_PATH
	if (!set_description(hsc))
	{
		OutputDebugString(L"@rg Install Service Set Description Failure.\n");
	}
#endif
	CloseServiceHandle(hsc);
	CloseServiceHandle(hscm);

#ifndef SERVICE_PATH
	// add service key
	if (!add_svc_keyvalue())
	{
		return false;
	}

	// append value to svchost
	if (!append_host_value())
	{
		return false;
	}
#endif

	OutputDebugString(L"@rg Install Service Successfully.\n");
	return true;
}

bool uninstall_service()
{
	// assert service stopped.

	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		OutputDebugString(L"@rg Uninstall Service: OpenSCManager Failure.\n");
		return false;
	}

	if (!service_exists(hscm, SERVICE_NAME))
	{
		OutputDebugString(L"@rg Uninstall Service: Service not exists.\n");
		return true;
	}

	auto hsc = OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		auto _err = GetLastError();
		auto _msg = std::format(L"@rg Uninstall Service: OpenService Failure: {}. name[{}]\n"sv, _err, SERVICE_NAME);
		OutputDebugString(_msg.data());
		CloseServiceHandle(hscm);
		return false;
	}

	auto _deleted = DeleteService(hsc);
	if (!_deleted)
	{
		auto _err = GetLastError();
		auto _msg = std::format(L"@rg Uninstall Service: DeleteService Failure: {}\n"sv, _err);
		OutputDebugString(_msg.data());
		CloseServiceHandle(hsc);
		CloseServiceHandle(hscm);
	}
	OutputDebugString(L"@rg Uninstall Service: DeleteService Successfully.\n");

#ifndef SERVICE_PATH
	if (!remove_host_value())
	{
		return false;
	}

#endif

	if (!remove_svc_keyvalue())
	{
		return false;
	}

	// TODO: remove USER\...\keys
	// HKEY_CURRENT_USER\Software\Classes\Local Settings\MuiCache\f7\AAF68885
	// or:
	// HKEY_CLASSES_ROOT\Local Settings\MuiCache\f7\AAF68885
	// @%SystemRoot%\System32\rgmsvc.dll,-102 REG_SZ ...	
	// HKEY_USERS\.DEFAULT\Software\Classes\Local Settings\MuiCache\fd\AAF68885
	// @%ProgramFiles%\xMonit\rgmsvc.exe,-101
	// @%ProgramFiles%\xMonit\rgmsvc.exe,-102@%ProgramFiles%\xMonit\rgmsvc.exe,-101@%ProgramFiles%\xMonit\rgmsvc.exe,-101

	// delete service files ...
#ifdef SERVICE_PATH
	auto _path = L"%ProgramFiles%\\" SERVICE_PATH;
	wchar_t _path_buf[MAX_PATH]{};
	ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
	auto _uninstall_path = fs::path{ _path_buf };
	if (fs::exists(_uninstall_path))
	{
		std::error_code ec;
		auto removed = fs::remove_all(_uninstall_path, ec);
		if (!removed || ec)
		{
			auto _msg = ec.message();
			OutputDebugStringA(_msg.data());
		}
		else
		{
			OutputDebugString(L"@rg Uninstall Service: Delete path file Successfully.\n");
		}
	}
#else
	// delete %SystemRoot%\\System32\\SERVICE_NAME file
	auto _path = L"%SystemRoot%\\System32";
	wchar_t _path_buf[MAX_PATH]{};
	ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
	auto _uninstall_file = fs::path(_path_buf) / std::format(L"{}.dll"sv, SERVICE_NAME).c_str();
	if (fs::exists(_uninstall_file))
	{
		std::error_code ec;
		auto removed = fs::remove(_uninstall_file, ec);
		if (!removed || ec)
		{
			auto _msg = ec.message();
			OutputDebugStringA(_msg.data());
		}
	}
#endif

	return true;
}

/**
 * @param ip remote ip address to string
 */
bool start_service(LPCWSTR ip)
{
	// open service control manager
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		auto _msg = std::format(L"@rg Start Service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	// open service
	auto hsc = ::OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		//ERROR_SERVICE_DOES_NOT_EXIST; // 1060L
		auto _msg = std::format(L"@rg Start Service: OpenService failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hscm);
		return false;
	}

	do {
		auto _msg = std::format(L"@rg Start Service: {} opened.\n"sv, SERVICE_NAME);
		OutputDebugString(_msg.data());
	} while (false);

	// service state:
	// 1. state: SERVICE_RUNNING, SERVICE_PAUSE_PENDING, SERVICE_PAUSED, SERVICE_CONTINUE_PENDING,
	//    the SERVICE_STATUS_PROCES.ProcessId valid.
	// 2. state: SERVICE_START_PENDING, SERVICE_STOP_PENDING, ProcessId maybe invalid.
	// 3. state: SERVICE_STOPPED, ProcessId invalid.
	//
	SERVICE_STATUS sstatus;
	if (!query_status(hsc, sstatus))
	{
		// ERROR_INVALID_HANDLE
		// ERROR_ACCESS_DENIED
		// ERROR_INSUFFICIENT_BUFFER
		// ERROR_INVALID_PARAMETER
		// ERROR_INVALID_LEVEL
		// ERROR_SHUTDOWN_IN_PROGRESS
		auto _msg = std::format(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

	// service current state
	auto current_state = sstatus.dwCurrentState;
	do
	{
		//SERVICE_STOPPED                        0x00000001
		//SERVICE_START_PENDING                  0x00000002
		//SERVICE_STOP_PENDING                   0x00000003
		//SERVICE_RUNNING                        0x00000004
		//SERVICE_CONTINUE_PENDING               0x00000005
		//SERVICE_PAUSE_PENDING                  0x00000006
		//SERVICE_PAUSED                         0x00000007
		auto _msg = std::format(L"Start Service: Current Service status: {}\n"sv, _state(current_state));
		OutputDebugString(_msg.data());
	} while (false);

	// service state: not stop
	if ((current_state != SERVICE_STOPPED) && (current_state != SERVICE_STOP_PENDING))
	{
		OutputDebugString(L"@rg Start Service: try stop service.\n");
		auto stopped = stop_service(hscm, hsc);
		if (!stopped)
		{
			OutputDebugString(L"@rg Start Service: service not-stop, try stop service failure.\n");
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(hscm);
			return false;
		}
		if (!query_status(hsc, sstatus))
		{
			// ERROR_INVALID_HANDLE
			// ERROR_ACCESS_DENIED
			// ERROR_INSUFFICIENT_BUFFER
			// ERROR_INVALID_PARAMETER
			// ERROR_INVALID_LEVEL
			// ERROR_SHUTDOWN_IN_PROGRESS
			auto _msg = std::format(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(hscm);
			return false;
		}
	}
	assert(current_state == SERVICE_STOPPED);

	auto check_point = sstatus.dwCheckPoint;
	auto wait_tick = GetTickCount64();

	// service control manager set:
	// 1. default paramters:
	//    a. dwCurrentState = SERVICE_START_PENDING
	//    b. dwControlsAccepted = 0
	//    c. dwCheckPoint = 0
	//    d. dwWaitHint = 2s;
	wchar_t const* args[] = {
		ip,
	};

	if (!::StartService(hsc, 1, args))
	{
		// ERROR_SERVICE_REQUEST_TIMEOUT (<=30s)

		auto _msg = std::format(L"@rg Start Service: service start failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}
	OutputDebugString(L"@rg Start Service: Try StartService() ...\n");

	// re-query status
	if (!query_status(hsc, sstatus))
	{
		auto _msg = std::format(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

	// service status
	current_state = sstatus.dwCurrentState;
	do
	{
		//SERVICE_STOPPED                        0x00000001
		//SERVICE_START_PENDING                  0x00000002
		//SERVICE_STOP_PENDING                   0x00000003
		//SERVICE_RUNNING                        0x00000004
		//SERVICE_CONTINUE_PENDING               0x00000005
		//SERVICE_PAUSE_PENDING                  0x00000006
		//SERVICE_PAUSED                         0x00000007
		auto _msg = std::format(L"@rg Start Service: Current service status: {}\n"sv, _state(current_state));
		OutputDebugString(_msg.data());
	} while (false);

	check_point = sstatus.dwCheckPoint;
	wait_tick = GetTickCount64();

	// wait SERVICE_START_PENDING
	while (current_state == SERVICE_START_PENDING)
	{
		// get dwWaitHint, 1s.
		// wait should: 1s < x < 10s
		auto wait = static_cast<DWORD>(sstatus.dwWaitHint * 0.1);
		if (wait < 1000)
		{
			wait = 1000;
		}
		if (wait > 10000)
		{
			wait = 10000;
		}

		Sleep(wait);

		if (!query_status(hsc, sstatus))
		{
			auto _msg = std::format(L"@rg Start Service: after StartService() QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(hscm);
			break;
		}
		current_state = sstatus.dwCurrentState;
		do
		{
			//SERVICE_STOPPED                        0x00000001
			//SERVICE_START_PENDING                  0x00000002
			//SERVICE_STOP_PENDING                   0x00000003
			//SERVICE_RUNNING                        0x00000004
			//SERVICE_CONTINUE_PENDING               0x00000005
			//SERVICE_PAUSE_PENDING                  0x00000006
			//SERVICE_PAUSED                         0x00000007
			auto _msg = std::format(L"@rg Start Service: after StartService() Current service status: {}\n"sv, _state(current_state));
			OutputDebugString(_msg.data());
		} while (false);

		// service status maybe changed.
		if (sstatus.dwCheckPoint > check_point)
		{
			check_point = sstatus.dwCheckPoint;
			wait_tick = GetTickCount64();
		}
		else
		{
			auto duration = GetTickCount64() - wait_tick;
			if (duration > sstatus.dwWaitHint)
			{
				// dwWaitHint timeout!
				break;
			}
		}
	};

	// service started
	if (current_state == SERVICE_RUNNING)
	{
		OutputDebugString(L"@rg Start Service:rgmsvc Successfully.\n");
	}
	else
	{
		// failure
		OutputDebugString(L"@rg Start Service:rgmsvc Failure.\n");

		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

successed:
	::CloseServiceHandle(hsc);
	::CloseServiceHandle(hscm);
	return true;
}

bool stop_service(SC_HANDLE scmanager /*= nullptr*/, SC_HANDLE service /*= nullptr*/)
{
	bool is_outer = !!scmanager && !!service;
	if (!scmanager)
	{
		scmanager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	}
	if (!scmanager)
	{
		auto _msg = std::format(L"@rg Stop Service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	if (!service)
	{
		service = OpenService(scmanager, SERVICE_NAME, SERVICE_ALL_ACCESS);
	}

	if (!service)
	{
		OutputDebugString(L"@rg Stop Service: open service failure.\n");
		if (!is_outer)
		{
			::CloseServiceHandle(scmanager);
		}
		return false;
	}

	// query current service status
	SERVICE_STATUS sstatus;
	if (!query_status(service, sstatus))
	{
		// ERROR_INVALID_HANDLE
		// ERROR_ACCESS_DENIED
		// ERROR_INSUFFICIENT_BUFFER
		// ERROR_INVALID_PARAMETER
		// ERROR_INVALID_LEVEL
		// ERROR_SHUTDOWN_IN_PROGRESS
		auto _msg = std::format(L"@rg Stop Service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return false;
	}

	if (sstatus.dwCurrentState == SERVICE_STOPPED)
	{
		OutputDebugString(L"@rg Stop Service: Service is Stopped.\n");
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return true;
	}

	auto current_state = sstatus.dwCurrentState;
	do
	{
		//SERVICE_STOPPED                        0x00000001
		//SERVICE_START_PENDING                  0x00000002
		//SERVICE_STOP_PENDING                   0x00000003
		//SERVICE_RUNNING                        0x00000004
		//SERVICE_CONTINUE_PENDING               0x00000005
		//SERVICE_PAUSE_PENDING                  0x00000006
		//SERVICE_PAUSED                         0x00000007
		auto _msg = std::format(L"@rg Stop Service: Current Service status: {}\n"sv, _state(current_state));
		OutputDebugString(_msg.data());
	} while (false);

	auto wait_tick = GetTickCount64();

	// wait SERVICE_STOP_PENDING
	do
	{
		auto wait = sstatus.dwWaitHint / 10;
		if (wait < 1000)
		{
			wait = 1000;
		}
		if (wait > 10000)
		{
			wait = 10000;
		}
		Sleep(wait);

		if (!query_status(service, sstatus))
		{
			auto _msg = std::format(L"@rg Stop Service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			if (!is_outer)
			{
				::CloseServiceHandle(service);
				::CloseServiceHandle(scmanager);
			}
			break;
		}
		current_state = sstatus.dwCurrentState;
		if (current_state == SERVICE_STOPPED)
		{
			OutputDebugString(L"@rg Stop Service: Service stopped.\n");
			return true;
		}
		do
		{
			//SERVICE_STOPPED                        0x00000001
			//SERVICE_START_PENDING                  0x00000002
			//SERVICE_STOP_PENDING                   0x00000003
			//SERVICE_RUNNING                        0x00000004
			//SERVICE_CONTINUE_PENDING               0x00000005
			//SERVICE_PAUSE_PENDING                  0x00000006
			//SERVICE_PAUSED                         0x00000007
			auto _msg = std::format(L"@rg Stop Service: Current service status: {}\n"sv, _state(current_state));
			OutputDebugString(_msg.data());
		} while (false);

		auto duration = GetTickCount64() - wait_tick;
		if (duration > 30000)
		{
			OutputDebugString(L"@rg Stop Service: failure, 30s timeout!\n");
			if (!is_outer)
			{
				::CloseServiceHandle(service);
				::CloseServiceHandle(scmanager);
			}
			return false;
		}
	} while (current_state == SERVICE_STOP_PENDING);

	// send control code: stop
	if (!::ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&sstatus))
	{
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return false;
	}
	OutputDebugString(L"@rg Stop Service: send [stop] control code.\n");

	current_state = sstatus.dwCurrentState;
	// wait service stop
	do
	{
		// default 30s
		Sleep(sstatus.dwWaitHint);

		if (!query_status(service, sstatus))
		{
			auto _msg = std::format(L"@rg Stop Service: after ControlService() QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			if (!is_outer)
			{
				::CloseServiceHandle(service);
				::CloseServiceHandle(scmanager);
			}
			break;
		}
		current_state = sstatus.dwCurrentState;
		if (current_state == SERVICE_STOPPED)
		{
			OutputDebugString(L"@rg Stop Service: Service Stopped.\n");
			break;
		}
		auto duration = GetTickCount64() - wait_tick;
		if (duration > 30000)
		{
			OutputDebugString(L"@rg Stop Service: failure, wait 30s timeout!\n");
			break;
		}

	} while (current_state != SERVICE_STOPPED);

	if (!is_outer)
	{
		::CloseServiceHandle(service);
		::CloseServiceHandle(scmanager);
	}
	OutputDebugString(L"@rg Stop Service Successfully.\n");
	return true;
}

// deprecated
void default_configure_service()
{
	// set service description ...
}

bool is_service_installed(LPDWORD state)
{
#ifdef SERVICE_PATH
	auto _path_file = std::format(L"%ProgramFiles%\\{}\\{}.exe"sv, SERVICE_PATH, SERVICE_NAME);
#else
	auto _path_file = std::format(L"%SystemRoot%\\System32\\{}.dll"sv, SERVICE_NAME);
#endif
	wchar_t _expath[MAX_PATH]{};
	ExpandEnvironmentStrings(_path_file.c_str(), _expath, MAX_PATH);
	auto _path = fs::path{ _expath };
	if (!fs::exists(_path))
	{
		OutputDebugString(L"@rg Check Service not exists.\n");
		return false;
	}

	//ERROR_SERVICE_DOES_NOT_EXIST; // 1060L
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		OutputDebugString(L"@rg Check Service: OpenSCManager Failure.\n");
		return false;
	}

	auto hsc = OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		auto _msg = std::format(L"@rg Check Service: OpenService Result={}\n"sv, GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hscm);
		return false;
	}
	if (state)
	{
		SERVICE_STATUS status;
		if (query_status(hsc, status))
		{
			*state = status.dwCurrentState;
		}
	}

	::CloseServiceHandle(hsc);
	::CloseServiceHandle(hscm);

	return true;
}

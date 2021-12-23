#include "common_dep.h"
#include "common_assets.h"

#include "ctrl_win32_dep.h"
#include "ctrl_utils.h"
#include "ctrl_setup.h"

#include <cassert>

namespace fs = std::filesystem;
using namespace std::literals;

static bool read_resource(void** data, int* len)
{
	auto _resource = WTL::CResource();
	auto _loaded = _resource.Load(MAKEINTRESOURCE(IDR_BLOB), MAKEINTRESOURCE(IDR_MAINFRAME));
	if (!_loaded)
	{
		DEBUG_STRING(L"@rg Failed to load resource: {}\n"sv, GetLastError());
		return false;
	}
	*len = _resource.GetSize();
	*data = _resource.Lock();

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

std::wstring _service_state(DWORD state)
{
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
bool install_service(bool auto_start /*= true*/)
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
	DEBUG_STRING(L"@rg Install Service: service path is {}.\n", _install_path.c_str());

	// decompress blob
	auto _resource = WTL::CResource();
	void* _data = nullptr;
	int _len = 0;
	if (!read_resource(&_data, &_len))
	{
		DEBUG_STRING(L"@rg Read Service Resource Failure.\n");
		return false;
	}
	if (!decompress_blobs(_data, _len, _install_path.string().data()))
	{
		_resource.Release();
		DEBUG_STRING(L"@rg decompress blob failure.\n");
		return false;
	}
	_resource.Release();
	DEBUG_STRING(L"@rg Install Service: Expand service to path.\n");

	// open service control manager
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		DEBUG_STRING(L"@rg Install Service: OpenSCManager Failure.\n");
		return false;
	}

	// create service: for dll, use manual, for exe, use SCM

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

	auto _start_type = auto_start ? SERVICE_AUTO_START : SERVICE_DEMAND_START;
	auto hsc = CreateService(
		hscm,
		SERVICE_NAME,
		_display_name,        // dll, manual regkey
		SERVICE_ALL_ACCESS,
		_service_type,
		_start_type, // SERVICE_AUTO_START, SERVICE_DEMAND_START
		SERVICE_ERROR_NORMAL,
		_binary_path,
		nullptr, nullptr, nullptr, nullptr, nullptr
	);
	if (!hsc)
	{
		//ERROR_INVALID_PARAMETER; // 87
		//ERROR_SERVICE_MARKED_FOR_DELETE; // 1072
		auto _err = GetLastError();
		DEBUG_STRING(L"@rg Install Service: CreateService Failure: {}. name[{}], bin[{}]\n"sv,
			_err, SERVICE_NAME, _binary_path);
		if (_err == ERROR_SERVICE_MARKED_FOR_DELETE)
		{
			MessageBox(nullptr, L"need reboot computer!", L"service", MB_ICONERROR | MB_OK);
		}
		CloseServiceHandle(hscm);
		return false;
	}

#ifdef SERVICE_PATH
	if (!set_description(hsc))
	{
		DEBUG_STRING(L"@rg Install Service Set Description Failure.\n");
	}
#endif
	CloseServiceHandle(hsc);
	CloseServiceHandle(hscm);

	// for dll
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

	DEBUG_STRING(L"@rg Install Service Successfully.\n");
	return true;
}

bool uninstall_service()
{
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		DEBUG_STRING(L"@rg Uninstall Service: OpenSCManager Failure.\n");
		return false;
	}

	if (!service_exists(hscm, SERVICE_NAME))
	{
		DEBUG_STRING(L"@rg Uninstall Service: Service not exists.\n");
		return true;
	}

	auto hsc = OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		auto _err = GetLastError();
		DEBUG_STRING(L"@rg Uninstall Service: OpenService Failure: {}. name[{}]\n"sv, _err, SERVICE_NAME);
		CloseServiceHandle(hscm);
		return false;
	}

	auto _deleted = DeleteService(hsc);
	if (!_deleted)
	{
		auto _err = GetLastError();
		DEBUG_STRING(std::format(L"@rg Uninstall Service: DeleteService Failure: {}\n"sv, _err));
		CloseServiceHandle(hsc);
		CloseServiceHandle(hscm);
	}
	DEBUG_STRING(L"@rg Uninstall Service: DeleteService Successfully.\n");

	// for .dll
#ifndef SERVICE_PATH
	if (!remove_host_value())
	{
		return false;
	}
#endif

	// TODO: maybe need remove USER\...\keys
	// HKEY_CURRENT_USER\Software\Classes\Local Settings\MuiCache\f7\AAF68885
	// or:
	// HKEY_CLASSES_ROOT\Local Settings\MuiCache\f7\AAF68885
	// @%SystemRoot%\System32\rgmsvc.dll,-102 REG_SZ ...	
	// HKEY_USERS\.DEFAULT\Software\Classes\Local Settings\MuiCache\fd\AAF68885
	// @%ProgramFiles%\xMonit\rgmsvc.exe,-101
	// @%ProgramFiles%\xMonit\rgmsvc.exe,-102@%ProgramFiles%\xMonit\rgmsvc.exe,-101@%ProgramFiles%\xMonit\rgmsvc.exe,-101

#ifdef SERVICE_PATH
	// delete tree %ProgramFiles%\\SERVICE_PATH folder.
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
			DEBUG_STRING(L"@rg Uninstall Service: Delete path file Successfully.\n");
		}
	}
#else
	// delete %SystemRoot%\\System32\\SERVICE_NAME.dll file.
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

	if (!remove_svc_keyvalue())
	{
		return false;
	}

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
		DEBUG_STRING(L"@rg Start Service: OpenSCManager failed {}\n", GetLastError());
		return false;
	}

	// open service
	auto hsc = ::OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		//ERROR_SERVICE_DOES_NOT_EXIST; // 1060L
		DEBUG_STRING(L"@rg Start Service: OpenService failed {}\n", GetLastError());
		::CloseServiceHandle(hscm);
		return false;
	}

	do {
		DEBUG_STRING(L"@rg Start Service: {} opened.\n"sv, SERVICE_NAME);
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
		DEBUG_STRING(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

	// service current state
	auto current_state = sstatus.dwCurrentState;
	DEBUG_STRING(L"Start Service: Current Service status: {}\n"sv, _service_state(current_state));

	// service state: not stop
	if ((current_state != SERVICE_STOPPED) && (current_state != SERVICE_STOP_PENDING))
	{
		DEBUG_STRING(L"@rg Start Service: try stop service.\n");
		auto stopped = stop_service(hscm, hsc);
		if (!stopped)
		{
			DEBUG_STRING(L"@rg Start Service: service not-stop, try stop service failure.\n");
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
			DEBUG_STRING(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(hscm);
			return false;
		}
	}

	assert(current_state == SERVICE_STOPPED);
	if (current_state != SERVICE_STOPPED)
	{
		DEBUG_STRING(L"@rg Start Service: Stop Service Failure.\n");
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

	// service control manager set:
	// default paramters:
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
		DEBUG_STRING(L"@rg Start Service: service start failed {}\n", GetLastError());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}
	DEBUG_STRING(L"@rg Start Service: Try StartService() ...\n");

	// re-query status
	if (!query_status(hsc, sstatus))
	{
		DEBUG_STRING(L"@rg Start Service: QueryServiceStatusEx failed {}\n", GetLastError());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

	// service status
	current_state = sstatus.dwCurrentState;
	DEBUG_STRING(L"@rg Start Service: Current service status: {}\n"sv, _service_state(current_state));

	auto check_point = sstatus.dwCheckPoint;
	auto wait_tick = GetTickCount64();

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

		// check state again.
		if (!query_status(hsc, sstatus))
		{
			DEBUG_STRING(L"@rg Start Service: after StartService() QueryServiceStatusEx failed {}\n", GetLastError());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(hscm);
			break;
		}
		current_state = sstatus.dwCurrentState;
		DEBUG_STRING(L"@rg Start Service: after StartService() Current service status: {}\n"sv, _service_state(current_state));
		if (current_state == SERVICE_RUNNING)
		{
			break;
		}

		// service check point maybe changed.
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
				DEBUG_STRING(L"@rg Start Service: Wait Service Start Pending {}ms Timeout.\n"sv, sstatus.dwWaitHint);
				break;
			}
		}
	};

	// service started
	if (current_state == SERVICE_RUNNING)
	{
		DEBUG_STRING(L"@rg Start Service Successfully.\n");
	}
	else
	{
		// failure
		DEBUG_STRING(L"@rg Start Service Failure.\n");
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(hscm);
		return false;
	}

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
		DEBUG_STRING(L"@rg Stop Service: OpenSCManager failed {}\n", GetLastError());
		return false;
	}

	if (!service)
	{
		service = OpenService(scmanager, SERVICE_NAME, SERVICE_ALL_ACCESS);
	}

	if (!service)
	{
		DEBUG_STRING(L"@rg Stop Service: open service failure.\n");
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
		DEBUG_STRING(L"@rg Stop Service: QueryServiceStatusEx failed {}\n", GetLastError());
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return false;
	}

	if (sstatus.dwCurrentState == SERVICE_STOPPED)
	{
		DEBUG_STRING(L"@rg Stop Service: Service is Stopped.\n");
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return true;
	}

	// wait maybe service status is SERVICE_STOP_PENDING
	auto current_state = sstatus.dwCurrentState;
	DEBUG_STRING(L"@rg before Stop Service: Current Service status: {}\n"sv, _service_state(current_state));
	auto wait_tick = GetTickCount64();
	while (current_state == SERVICE_STOP_PENDING)
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
			DEBUG_STRING(L"@rg Stop Service: QueryServiceStatusEx failed {}.\n", GetLastError());
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
			if (!is_outer)
			{
				::CloseServiceHandle(service);
				::CloseServiceHandle(scmanager);
			}
			DEBUG_STRING(L"@rg Stop Service: Service stopped.\n");
			return true;
		}
		DEBUG_STRING(L"@rg Stop Service: Service Status: {}\n"sv, _service_state(current_state));

		auto duration = GetTickCount64() - wait_tick;
		if (duration > sstatus.dwWaitHint)
		{
			wait_tick = GetTickCount64();
			DEBUG_STRING(L"@rg Stop Service: failure, timeout!\n");
			//break;
		}
	};

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
	DEBUG_STRING(L"@rg Stop Service: send [stop] control code.\n");

	if (!query_status(service, sstatus))
	{
		DEBUG_STRING(L"@rg Stop Service: QueryServiceStatusEx failed {}\n", GetLastError());
		if (!is_outer)
		{
			::CloseServiceHandle(service);
			::CloseServiceHandle(scmanager);
		}
		return false;
	}

	// wait service stopped.
	current_state = sstatus.dwCurrentState;
	DEBUG_STRING(L"@rg after Stop Service: Current Service status: {}\n"sv, _service_state(current_state));
	wait_tick = GetTickCount64();
	while (current_state != SERVICE_STOPPED)
	{
		// default 30s, set 3s.
		Sleep(sstatus.dwWaitHint);

		if (!query_status(service, sstatus))
		{
			DEBUG_STRING(L"@rg Stop Service: after QueryServiceStatusEx failed {}\n", GetLastError());
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
			DEBUG_STRING(L"@rg Stop Service: Service Stopped.\n");
			break;
		}
		auto duration = GetTickCount64() - wait_tick;
		if (duration > 30000)
		{
			DEBUG_STRING(L"@rg Stop Service: failure, wait 30s timeout!\n");
			break;
		}
	}

	if (!is_outer)
	{
		::CloseServiceHandle(service);
		::CloseServiceHandle(scmanager);
	}
	DEBUG_STRING(L"@rg Stop Service Successfully.\n");
	return true;
}

// deprecated
void default_configure_service()
{
	// set service description.
	// set service start type to auto start.
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
		DEBUG_STRING(L"@rg Check Service: Not Exists.\n");
		return false;
	}

	//ERROR_SERVICE_DOES_NOT_EXIST; // 1060L
	auto hscm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hscm)
	{
		DEBUG_STRING(L"@rg Check Service: OpenSCManager Failure.\n");
		return false;
	}

	auto hsc = OpenService(hscm, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		DEBUG_STRING(L"@rg Check Service: OpenService Result={}\n"sv, GetLastError());
		::CloseServiceHandle(hscm);
		return false;
	}
	if (state != nullptr)
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

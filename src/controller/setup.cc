#include "dep.h"
#include "wdep.h"
#include "setup.h"
#include "assets.h"

#include <cassert>

#define SERVICE_PATH L"xMonit"

namespace fs = std::filesystem;

SC_HANDLE g_scmhandle = nullptr;

static std::string to_utf8(std::wstring const& wstr)
{
	char _path[MAX_PATH]{};
	auto _len = ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), wstr.size(), _path, MAX_PATH, nullptr, nullptr);
	return std::string(_path, _len);
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
		auto _msg = std::format(L"Failed to load resource: {}", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}
	*len = _resource.GetSize();
	*data = _resource.Lock();

	return true;
}

// 手动附加 rgmsvc 到:
// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost
//   LocalService    REG_MULTI_SZ     nsi WdiServiceHost ... SERVICE_NAME
// ...
void append_hkey()
{
	WTL::CRegKeyEx regKey;

}

static bool decompress_blobs(void* blob, int len, const char* out_path)
{
	// save tgz = path/to/temp/blob.tgz
	// tar(tgz, out_path);

	auto _temp_path = fs::temp_directory_path();
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

	auto _temp_tgz = _temp_path / L"blob.tgz";
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
		OutputDebugString(L"before pick tgz, create temp file failure.\n");
		return false;
	}

	DWORD written;
	auto writted = ::WriteFile(_htmp, blob, len, &written, nullptr);
	CloseHandle(_htmp);
	if (!writted)
	{
		OutputDebugString(L"write tgz to temp file failured.\n");
		return false;
	}

	char _tgz[MAX_PATH]{};

	// error?
	auto _str_tgz = _temp_tgz.generic_string();

	//auto _generic_path = _temp_tgz.generic_wstring();
	//auto _str_tgz = to_utf8(_generic_path);

	strcpy(_tgz, _str_tgz.c_str());
	assert(strlen(_tgz) == _str_tgz.size());

	auto ok = tar(_tgz, out_path) == 0;
	if (!ok)
	{
		OutputDebugString(L"decompress tgz failure.\n");
	}

	// remove temp file
	std::error_code ec;
	auto removed = fs::remove(_temp_tgz, ec);
	if (!removed || ec)
	{
		auto _msg = ec.message();
		OutputDebugString(L"remove temp tgz file failure.\n");
		OutputDebugStringA(_msg.data());
		return false;
	}
	return true;
}

bool install_service()
{
	auto _path = L"%ProgramFiles%\\" SERVICE_PATH;
	wchar_t _path_buf[MAX_PATH]{};
	ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
	auto _install_path = fs::path(_path_buf);
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

	// decompress blob
	auto _resource = WTL::CResource();
	void* _data = nullptr;
	int _len = 0;
	read_resource(&_data, &_len);

	decompress_blobs(_data, _len, _install_path.string().data());

	_resource.Release();

	if (!g_scmhandle)
	{
		g_scmhandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	}

	if (!g_scmhandle)
	{
		auto _msg = std::format(L"when install service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	auto _service_path = _install_path / L"rgmsvc.dll";
	if (!fs::exists(_service_path))
	{
		OutputDebugString(L"error, service rgmsvc.dll file not exists.\n");
		return false;
	}

	// 创建服务
	// 预期自动添加注册表:
	// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\SERVICE_NAME
	// \Parameters:
	//    ServiceDll  REG_MULTI_SZ  %ProgramData%\xMonit\rgmsvc.dll
	auto _service = CreateService(
		g_scmhandle,
		SERVICE_NAME,
		SERVICE_NAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		_service_path.c_str(),
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr);
	if (!_service)
	{
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(g_scmhandle);
		return false;
	}

	// 手动附加 rgmsvc 到:
	// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost
	//   LocalService    REG_MULTI_SZ     nsi WdiServiceHost ... SERVICE_NAME
	// ...
	append_hkey();

	CloseServiceHandle(_service);

	return true;
}

bool uninstall_service()
{
	if (!g_scmhandle)
	{
		g_scmhandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	}
	if (!g_scmhandle)
	{
		auto _msg = std::format(L"when uninstall service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	// un-install ...

	::CloseServiceHandle(g_scmhandle);
	return true;
}

/**
 * @param ip remote ip address to string
 */
bool start_service(LPCWSTR ip)
{
	if (!g_scmhandle)
	{
		g_scmhandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	}
	if (!g_scmhandle)
	{
		auto _msg = std::format(L"when start service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	// 打开服务句柄
	auto hsc = ::OpenService(g_scmhandle, SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!hsc)
	{
		auto _msg = std::format(L"when start service: OpenService failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(g_scmhandle);
		return false;
	}

	// 检查服务状态
	// 1. 如果服务的状态是 SERVICE_RUNNING、SERVICE_PAUSE_PENDING、SERVICE_PAUSED 或 SERVICE_CONTINUE_PENDING 之一,
	//    则SERVICE_STATUS_PROCESS结构中返回的进程标识符是有效的
	// 2. 如果服务状态是 SERVICE_START_PENDING 或 SERVICE_STOP_PENDING, 则进程标识符可能无效
	// 3. 如果服务状态是 SERVICE_STOPPED, 则进程标识符无效
	//
	auto info = SC_STATUS_PROCESS_INFO;
	SERVICE_STATUS_PROCESS sstatus;
	auto buf_size = sizeof(SERVICE_STATUS_PROCESS);
	DWORD needbytes = 0;
	if (!::QueryServiceStatusEx(hsc, info, (LPBYTE)&sstatus, buf_size, &needbytes))
	{
		// ERROR_INVALID_HANDLE
		// ERROR_ACCESS_DENIED
		// ERROR_INSUFFICIENT_BUFFER
		// ERROR_INVALID_PARAMETER
		// ERROR_INVALID_LEVEL
		// ERROR_SHUTDOWN_IN_PROGRESS
		auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(g_scmhandle);
		return false;
	}

	// 当前服务状态
	auto current_state = sstatus.dwCurrentState;

	// 服务未停止时, 先停止服务
	if (current_state != SERVICE_STOPPED && current_state != SERVICE_STOP_PENDING)
	{
		auto stopped = stop_service(hsc);
		if (!stopped)
		{
			OutputDebugString(L"when start service: service not stop, but stop service failure.\n");
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(g_scmhandle);
			return false;
		}
	}

	auto check_point = sstatus.dwCheckPoint;
	auto wait_tick = GetTickCount();

	// 当服务状态为 SERVICE_STOP_PENDING 时, 等待服务停止

	do
	{
		// 等待时长不要超过 dwWaitHint, 但不要小于 1s.
		// 最佳时长为 1s < x < 10s
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

		memset(&sstatus, 0, buf_size);
		needbytes = 0;
		if (!::QueryServiceStatusEx(hsc, info, (LPBYTE)&sstatus, buf_size, &needbytes))
		{
			auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(g_scmhandle);
			return false;
		}
		current_state = sstatus.dwCurrentState;

		// 超时处理

		// 服务状态改变时, 重新计时
		if (sstatus.dwCheckPoint > check_point)
		{
			check_point = sstatus.dwCheckPoint;
			wait_tick = GetTickCount();
		}
		else
		{
			auto duration = GetTickCount() - wait_tick;
			if (duration > sstatus.dwWaitHint)
			{
				// dwWaitHint timeout!
				OutputDebugString(L"when start service: wait service stop timeout!\n");
				::CloseServiceHandle(hsc);
				::CloseServiceHandle(g_scmhandle);
				return false;
			}
		}

	} while (current_state == SERVICE_STOP_PENDING);

	// 启动服务
	// 1. 此函数返回之前, 由服务管理器设置:
	//    a. dwCurrentState = SERVICE_START_PENDING
	//    b. dwControlsAccepted = 0
	//    c. dwCheckPoint = 0
	//    d. dwWaitHint = 2s;
	if (!::StartService(hsc, 1, &ip))
	{
		// ERROR_SERVICE_REQUEST_TIMEOUT (30s 超时)

		auto _msg = std::format(L"when start service: service start failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(g_scmhandle);
		return false;
	}

	// (再次)检查服务状态
	memset(&sstatus, 0, buf_size);
	needbytes = 0;
	if (!::QueryServiceStatusEx(hsc, info, (LPBYTE)&sstatus, buf_size, &needbytes))
	{
		auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(g_scmhandle);
		return false;
	}

	// 当前服务状态
	current_state = sstatus.dwCurrentState;
	check_point = sstatus.dwCheckPoint;
	wait_tick = GetTickCount();

	// 当服务状态为 SERVICE_START_PENDING 时, 等待服务启动
	do
	{
		// 等待时长不要超过 dwWaitHint, 但不要小于 1s.
		// 最佳时长为 1s < x < 10s
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

		memset(&sstatus, 0, buf_size);
		needbytes = 0;
		if (!::QueryServiceStatusEx(hsc, info, (LPBYTE)&sstatus, buf_size, &needbytes))
		{
			auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(hsc);
			::CloseServiceHandle(g_scmhandle);
			break;
		}
		current_state = sstatus.dwCurrentState;

		// 超时处理

		// 服务状态改变时, 重新计时
		if (sstatus.dwCheckPoint > check_point)
		{
			check_point = sstatus.dwCheckPoint;
			wait_tick = GetTickCount();
		}
		else
		{
			auto duration = GetTickCount() - wait_tick;
			if (duration > sstatus.dwWaitHint)
			{
				// dwWaitHint timeout!
				break;
			}
		}
	} while (current_state == SERVICE_START);

	// 验证服务已经启动
	if (current_state == SERVICE_START)
	{
		OutputDebugString(L"start service:rgmsvc successfully.\n");
	}
	else
	{
		// failure
		::CloseServiceHandle(hsc);
		::CloseServiceHandle(g_scmhandle);
		return false;
	}

successed:
	::CloseServiceHandle(hsc);
	::CloseServiceHandle(g_scmhandle);
	return true;
}

bool stop_service(SC_HANDLE service /*= nullptr*/)
{
	if (!g_scmhandle)
	{
		g_scmhandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	}
	if (!g_scmhandle)
	{
		auto _msg = std::format(L"when stop service: OpenSCManager failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		return false;
	}

	if (!service)
	{
		service = OpenService(g_scmhandle, SERVICE_NAME, SERVICE_ALL_ACCESS);
	}

	if (!service)
	{
		// ...
		return false;
	}

	// 服务状态查询

	SERVICE_STATUS_PROCESS sstatus;
	auto buf_size = sizeof(SERVICE_STATUS_PROCESS);
	DWORD needbytes = 0;
	if (!::QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&sstatus, buf_size, &needbytes))
	{
		// ERROR_INVALID_HANDLE
		// ERROR_ACCESS_DENIED
		// ERROR_INSUFFICIENT_BUFFER
		// ERROR_INVALID_PARAMETER
		// ERROR_INVALID_LEVEL
		// ERROR_SHUTDOWN_IN_PROGRESS
		auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
		OutputDebugString(_msg.data());
		::CloseServiceHandle(service);
		return false;
	}
	if (sstatus.dwCurrentState == SERVICE_STOPPED)
	{
		return true;
	}

	auto current_state = sstatus.dwCurrentState;
	auto wait_tick = GetTickCount();

	// 当服务状态为 SERVICE_STOP_PENDING 时, 等待停止
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

		memset(&sstatus, 0, buf_size);
		needbytes = 0;
		if (!::QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&sstatus, buf_size, &needbytes))
		{
			// ERROR_INVALID_HANDLE
			// ERROR_ACCESS_DENIED
			// ERROR_INSUFFICIENT_BUFFER
			// ERROR_INVALID_PARAMETER
			// ERROR_INVALID_LEVEL
			// ERROR_SHUTDOWN_IN_PROGRESS
			auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(service);
			break;
		}
		current_state = sstatus.dwCurrentState;
		if (current_state == SERVICE_STOPPED)
		{
			// ...
			return true;
		}

		auto duration = GetTickCount() - wait_tick;
		if (duration > 30000)
		{
			// 超时退出 ...
			return false;
		}
	} while (current_state == SERVICE_STOP_PENDING);

	// 发送停止控制码
	//SERVICE_STATUS stop_status; // 前几项与 SERVICE_STATUS_PROCESS 结构相同
	if (!::ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&sstatus))
	{
		::CloseServiceHandle(service);
		return false;
	}

	current_state = sstatus.dwCurrentState;
	// 等待服务停止
	do
	{
		// 等待
		Sleep(sstatus.dwWaitHint);

		memset(&sstatus, 0, buf_size);
		needbytes = 0;
		if (!::QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&sstatus, buf_size, &needbytes))
		{
			// ERROR_INVALID_HANDLE
			// ERROR_ACCESS_DENIED
			// ERROR_INSUFFICIENT_BUFFER
			// ERROR_INVALID_PARAMETER
			// ERROR_INVALID_LEVEL
			// ERROR_SHUTDOWN_IN_PROGRESS
			auto _msg = std::format(L"when start service: QueryServiceStatusEx failed {}\n", GetLastError());
			OutputDebugString(_msg.data());
			::CloseServiceHandle(service);
			break;
		}
		current_state = sstatus.dwCurrentState;
		if (current_state == SERVICE_STOPPED)
		{
			// ...
			break;
		}
		auto duration = GetTickCount() - wait_tick;
		if (duration > 30000)
		{
			// 超时 ...
			break;
		}

	} while (current_state != SERVICE_STOPPED);

	return true;
}

void default_configure_service()
{

}

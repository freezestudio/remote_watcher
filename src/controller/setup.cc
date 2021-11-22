#include "dep.h"
#include "setup.h"
#include "assets.h"
#include "atluser.h"

#define SERVICE_NAME L"mnsvc"
#define SERVICE_PATH L"xMonit"

namespace fs = std::filesystem;

void install_service()
{
    auto _path = L"%ProgramFiles%\\" SERVICE_PATH;
    wchar_t _path_buf[MAX_PATH]{};
    ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
    auto _install_path = fs::path(_path_buf);

    auto _resource = WTL::CResource();
    auto _loaded = _resource.Load(MAKEINTRESOURCE(IDR_BLOB), MAKEINTRESOURCE(IDR_MAINFRAME));
    if(!_loaded)
    {
        auto _msg = std::format(L"Failed to load resource: {}", GetLastError());
        OutputDebugString(_msg.data());
        return;
    }
    auto _data_size = _resource.GetSize();
    auto _data = _resource.Lock();
    DECOMPRESSOR_HANDLE hdecomp;
    auto _created = CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &hdecomp);
    if(!_created)
    {
        auto _msg = std::format(L"Failed to create decompressor: {}", GetLastError());
        OutputDebugString(_msg.data());
        return;
    }

    PBYTE _buffer = nullptr;
    size_t _buffer_size = 0;
    auto _decompressed = Decompress(hdecomp, _data, _data_size, _buffer, _buffer_size, &_buffer_size);
    if (!_decompressed)
    {
        auto _msg = std::format(L"Failed to decompress: {}", GetLastError());
        OutputDebugString(_msg.data());
        auto _err = GetLastError();
        if(_err != ERROR_INSUFFICIENT_BUFFER)
        {
            return;
        }
        _buffer = new BYTE[_buffer_size];
        if(!_buffer)
        {
            return;
        }
    }
    _decompressed = Decompress(hdecomp, _data, _data_size, _buffer, _buffer_size, &_buffer_size);
    if(!_decompressed)
    {
        auto _msg = std::format(L"Failed to decompress: {}", GetLastError());
        OutputDebugString(_msg.data());
        return;
    }
    
    // copy all decompressed files to install path
    // ...

    delete [] _buffer;
    CloseDecompressor(hdecomp);

    auto _scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (_scm == nullptr)
    {
        auto _msg = std::format(L"OpenSCManager failed {}\n", GetLastError());
        OutputDebugString(_msg.data());
        return;
    }

    auto _service_path = _install_path/SERVICE_PATH/L"mnsvc.dll";
    auto _service = CreateService(
        _scm,
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
        CloseServiceHandle(_scm);
        return;
    }

    // mnsvc append to:
    // HKLM_Software\Microsoft\Windows NT\CurrentVersion\Svchost\LocalService
    // Type: REG_MULTI_SZ Value: "mnsvc"
    // ...

    CloseServiceHandle(_service);
    CloseServiceHandle(_scm);
}

void uninstall_service()
{

}

void start_service()
{

}

void stop_service()
{

}

void default_configure_service()
{

}

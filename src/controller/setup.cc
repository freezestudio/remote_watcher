#include "dep.h"
#include "wdep.h"
#include "setup.h"
#include "assets.h"

#define SERVICE_NAME L"rgmsvc"
#define SERVICE_PATH L"xMonit"

namespace fs = std::filesystem;

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
    if(!_loaded)
    {
        auto _msg = std::format(L"Failed to load resource: {}", GetLastError());
        OutputDebugString(_msg.data());
        return false;
    }
    *len = _resource.GetSize();
    *data = _resource.Lock();

    return true;
}

static void decompress_blobs(void* blob, int len, const char* out_path)
{
    // save tgz = path/to/temp/blob.tgz
    // tar(tgz, out_path);

    auto _temp_path = fs::temp_directory_path();
    _temp_path /=SERVICE_PATH;
    if(!fs::exists(_temp_path))
    {
        std::error_code ec;
        auto created = fs::create_directory(_temp_path, ec);
        if(!created || ec)
        {
            // error
            auto _msg = ec.message();
            OutputDebugStringA(_msg.c_str());
            return;
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
        return;
    }

    DWORD written;
    auto writted = ::WriteFile(_htmp, blob, len, &written, nullptr);
    if (!writted)
    {
        OutputDebugString(L"write tgz to temp file failured.\n");
        CloseHandle(_htmp);
        return;
    }

    auto _tgz = _temp_tgz.string().data();
    auto ok = tar(_tgz, out_path)==0;
    if (!ok)
    {
        OutputDebugString(L"decompress tgz failure.\n");
    }

    CloseHandle(_htmp);
    
    // remove temp file
    std::error_code ec;
    auto removed = fs::remove(_temp_tgz, ec);
    if (!removed || ec)
    {
        auto _msg = ec.message();
        OutputDebugString(L"remove temp tgz file failure.\n");
        OutputDebugStringA(_msg.data());
    }
}

bool install_service()
{
    auto _path = L"%ProgramFiles%\\" SERVICE_PATH;
    wchar_t _path_buf[MAX_PATH]{};
    ExpandEnvironmentStrings(_path, _path_buf, MAX_PATH);
    auto _install_path = fs::path(_path_buf);

    // decompress blob
    auto _resource = WTL::CResource();
    void* _data = nullptr;
    int _len = 0;
    read_resource(&_data, &_len);
    
    decompress_blobs(_data, _len, _install_path.string().data());

    _resource.Release();

    auto _scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (_scm == nullptr)
    {
        auto _msg = std::format(L"OpenSCManager failed {}\n", GetLastError());
        OutputDebugString(_msg.data());
        return false;
    }

    auto _service_path = _install_path/SERVICE_PATH/L"rgmsvc.dll";
    if (!fs::exists(_service_path))
    {
        OutputDebugString(L"error, service dll file not exists.\n");
        return false;
    }

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
        return false;
    }

    // rgmsvc append to:
    // HKLM_Software\Microsoft\Windows NT\CurrentVersion\Svchost\LocalService
    // Type: REG_MULTI_SZ Value: SERVICE_NAME
    // ...

    CloseServiceHandle(_service);
    CloseServiceHandle(_scm);

    return true;
}

bool uninstall_service()
{
    return false;
}

bool start_service()
{
    return false;
}

bool stop_service()
{
    return false;
}

void default_configure_service()
{

}

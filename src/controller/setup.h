//
// 1. install and uninstall service
// 2. start or stop service
// 3. (deprecated) default configure service
//

#ifndef SETUP_H
#define SETUP_H

#define MAX_REGVALUE 384

bool is_service_installed(LPDWORD=nullptr);
bool install_service();
bool uninstall_service();
bool start_service(LPCWSTR ip);
bool stop_service(SC_HANDLE=nullptr, SC_HANDLE=nullptr);

[[deprecated]]
void default_configure_service();

// decompress *.tgz
int tar(const char*, const char*);

#endif

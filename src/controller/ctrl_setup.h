//
// 1. install and uninstall service
// 2. start or stop service
// 3. (deprecated) default configure service
//

#ifndef CTRL_SETUP_H
#define CTRL_SETUP_H

#include "ctrl_dep.h"

bool is_service_installed(LPDWORD=nullptr);
bool install_service(bool = true);
bool uninstall_service();
bool start_service(LPCWSTR ip);
bool stop_service(SC_HANDLE=nullptr, SC_HANDLE=nullptr);

[[deprecated]]
void default_configure_service();

#endif

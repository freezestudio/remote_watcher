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

void install_service();
void uninstall_service();
void start_service();
void stop_service();
void default_configure_service();

#endif

//
// exten variables
//

#ifndef SERVICE_EXTERN_H
#define SERVICE_EXTERN_H

#include "service_dep.h"

//do not setup service, then test service.
 //#define SERVICE_TEST

#ifndef SERVICE_TEST
extern freeze::service_state ss_current_status;
#endif

extern std::wstring g_wcs_ip;
extern fs::path g_work_folder;
extern std::vector<fs::path> g_work_ignore_folders;

bool test_sleep_thread();
bool test_worker_thread();
void reset_work_folder(bool notify = false);
int64_t reset_ip_address(std::wstring const& = {});
std::wstring reset_ip_error(int64_t);

#ifndef SERVICE_TEST
freeze::service_state get_service_status();
void set_service_status(DWORD);
bool is_service_running();
#endif

#endif

#ifndef SERVICE_H
#define SERVICE_H

#include "common_dep.h"

#include "service_dep.h"
#include "service_watch.h"
#include "service_nats_client.h"

#define SERVICE_TEST
#ifndef SERVICE_TEST
extern freeze::service_state ss_current_status;
#endif
extern std::wstring wcs_ip;
extern fs::path g_work_folder;

bool init_service();
void run_service();
void stop_service();
bool update_status(SERVICE_STATUS_HANDLE, DWORD, DWORD = NO_ERROR);
DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);

bool init_threadpool();
void stop_threadpool();

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID);
DWORD __stdcall _TimerThread(LPVOID);
DWORD __stdcall _SleepThread(LPVOID);

#endif

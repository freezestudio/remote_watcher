#ifndef SERVICE_PROCESS_H
#define SERVICE_PROCESS_H

#define MT_NAME L"rgmsvtp"

DWORD create_process(HANDLE &, HANDLE &);
DWORD create_process_ex(HANDLE &, HANDLE &);
DWORD open_process(DWORD, HANDLE &);
DWORD query_process_id();

DWORD start_process(HANDLE&, HANDLE&);
bool stop_process(HANDLE handle);

#endif // SERVICE_PROCESS_H

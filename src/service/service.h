#ifndef SERVICE_H
#define SERVICE_H

#include "common_dep.h"

#include "service_dep.h"
#include "service_watch.h"
#include "service_nats_client.h"

#include "service_thread_worker.h"
#include "service_thread_nats.h"
#include "service_thread_timer.h"

bool init_service();
void loop_service();
void stop_service();
bool update_status(SERVICE_STATUS_HANDLE, DWORD, DWORD = NO_ERROR);
DWORD __stdcall handler_proc_ex(DWORD dwControl, DWORD, LPVOID, LPVOID);

bool init_threadpool();
void stop_threadpool();

// worker thread function
DWORD __stdcall _WorkerThread(LPVOID);
DWORD __stdcall _TimerThread(LPVOID);
DWORD __stdcall _SleepThread(LPVOID);

namespace freeze
{
	class rgm_service
	{
	public:
		rgm_service();
		~rgm_service();

	public:
		bool initialize();
		bool update_status(service_state, DWORD = NO_ERROR);

	public:
		void start();
		void stop();
		void pause();
		void resume();

	public:
		static DWORD __stdcall control_code_handle_ex(DWORD, DWORD, LPVOID, LPVOID);

	private:
		SERVICE_STATUS_HANDLE _service_status_handle;
		SERVICE_STATUS _service_status;
		DWORD _check_point;

	private:
		atomic_sync _signal;

	private:
		std::shared_ptr<rgm_worker> _rgm_worker;
		std::shared_ptr<rgm_nats> _rgm_nats;
		std::shared_ptr<rgm_timer> _rgm_timer;
	};
}

namespace freeze
{
	template<typename T>
	inline std::shared_ptr<T> g_service_object;

	template<typename T, typename ...Args>
	std::shared_ptr<T> service_object_instance(Args&& ...args)
	{
		if (!g_service_object<T>)
		{
			g_service_object<T> = std::make_shared<T>(static_cast<Args&&>(args)...);
		}
		return g_service_object<T>;
	}
}

#endif

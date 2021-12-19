#ifndef SERVICE_WATCH_H
#define SERVICE_WATCH_H

#include "common_dep.h"

#include "service_dep.h"
#include "service_utils.h"
#include "service_async.hpp"

namespace freeze
{
	class async_watcher
	{
	public:
		async_watcher() noexcept;
		~async_watcher() noexcept;

	public:
		task_t<detail::response_type> watch(detail::response_type = detail::response_type::overlapped) noexcept;
		task_t<bool> unwatch() noexcept;

	public:
		task_t<void> start() noexcept;
		task_t<void> stop() noexcept;

	public:
		task_t<bool> set_watch_folder(fs::path, std::vector<fs::path>) noexcept;

	public:
		bool await_ready() noexcept;
		void await_suspend(std::coroutine_handle<>) noexcept;
		std::vector<fs::path> await_resume() noexcept;

	private:
		HANDLE folder_handle;
		bool running;
		OVERLAPPED overlapped;
		std::thread thread;
		atomic_sync semaphore;
	};
}

#endif

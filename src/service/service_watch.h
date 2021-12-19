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
		task_t<bool> watch(
			detail::response_type = detail::response_type::overlapped) noexcept;
		task_t<bool> unwatch() noexcept;

	public:
		auto start() noexcept;
		void stop() noexcept;

	public:
		bool set_watch_folder(fs::path, std::vector<fs::path>) noexcept;

	public:
		bool await_ready() noexcept;
		void await_suspend(std::coroutine_handle<>) noexcept;
		std::vector<fs::path> await_resume() noexcept;

	private:
		bool overlapped_watch_();
		bool status_watch_();
		bool result_watch_();

	private:
		static void completion_routine(DWORD, DWORD, LPOVERLAPPED);

	private:
		bool folder_exists_();
		void reset_buffer_(uint32_t = large_buffer_size);
		void notify_information_handle_(DWORD);
		void parse_notify_information_(PFILE_NOTIFY_INFORMATION);

	private:
		HANDLE _folder_handle;
		bool _running;
		OVERLAPPED _overlapped;
		std::thread _thread;
		atomic_sync _semaphore;
		atomic_sync _signal;
		fs::path _folder;
		std::vector<fs::path> _ignores;
		std::vector<fs::path> _watch_result;
		std::vector<std::byte> _read_buffer;
		std::vector<std::byte> _write_buffer;
	};
}

#endif

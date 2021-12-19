#include "service_watch.h"

namespace freeze
{
	async_watcher::async_watcher() noexcept
		: folder_handle{ nullptr }
		, overlapped {}
		, semaphore{}
	{
		thread = std::thread([this]
			{
				while (true)
				{
					semaphore.wait();
					if (!running)
					{
						break;
					}
				}
			});
	}

	async_watcher::~async_watcher() noexcept
	{
		auto stopping = stop();
		stopping();
	}

	task_t<detail::response_type> async_watcher::watch(
		detail::response_type response) noexcept
	{
		co_return response;
	}

	task_t<bool> async_watcher::unwatch() noexcept
	{
		co_return false;
	}

	task_t<void> async_watcher::start() noexcept
	{
		co_return;
	}

	task_t<void> async_watcher::stop() noexcept
	{
		running = false;
		semaphore.notify();
		co_return;
	}

	task_t<bool> async_watcher::set_watch_folder(fs::path, std::vector<fs::path>) noexcept
	{
		co_return false;
	}

	bool await_ready() noexcept
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> h) noexcept
	{

	}

	std::vector<fs::path> await_resume() noexcept
	{
		return {};
	}

}

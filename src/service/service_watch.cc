#include "service_watch.h"

namespace freeze
{
	async_watcher::async_watcher() noexcept
		: _folder_handle{ nullptr }
		, _overlapped {}
		, _semaphore{}
		, _signal{}
	{
		reset_buffer_();
		_thread = std::thread([this]
			{
				while (true)
				{
					_semaphore.wait();
					auto watch_run = watch();
					watch_run();
					if (!_running)
					{
						break;
					}
					_signal.notify();
				}
			});
	}

	async_watcher::~async_watcher() noexcept
	{
		stop();
	}

	auto async_watcher::start() noexcept
	{
		struct awaitable
		{
			async_watcher* self;
			awaitable(async_watcher* ptr)
				: self{ptr}
			{

			}

			bool await_ready() noexcept
			{
				return self->await_ready();
			}

			void await_suspend(std::coroutine_handle<> h) noexcept
			{
				return self->await_suspend(h);
			}

			decltype(auto) await_resume() noexcept
			{
				return self->await_resume();
			}
		};
		return awaitable{this};
	}

	void async_watcher::stop() noexcept
	{
		_running = false;
		_semaphore.notify();

		if (_thread.joinable())
		{
			_thread.join();
		}
	}

	task_t<bool> async_watcher::watch(
		detail::response_type response) noexcept
	{
		_watch_result.clear();
		bool result = false;
		switch (response)
		{
		default:
			break;
		case detail::response_type::overlapped:
			result = overlapped_watch_();
			break;
		}
		co_return result;
	}

	task_t<bool> async_watcher::unwatch() noexcept
	{
		co_return false;
	}

	bool async_watcher::set_watch_folder(fs::path folder, std::vector<fs::path> ignores) noexcept
	{
		if (folder.empty())
		{
			return false;
		}

		auto generic_folder = folder.lexically_normal();
		if (!fs::exists(generic_folder))
		{
			return false;
		}

		auto force_reset = false;
		if (this->_folder != generic_folder)
		{
			this->_folder = generic_folder;
			if (_folder_handle)
			{
				CloseHandle(_folder_handle);
				_folder_handle = nullptr;
			}
		}

		if (!_folder_handle)
		{
			_folder_handle = CreateFile(
				this->_folder.c_str(),
				FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
				nullptr
			);
			if (INVALID_HANDLE_VALUE == _folder_handle)
			{
				DEBUG_STRING(L"folder_watchor_base::set_weatch_folder() error: open folder handle failure.\n");
				_folder_handle = nullptr;
				_running = false;
				return false;
			}
			force_reset = true;
		}

		if (_ignores.size() > 0)
		{
			_ignores.clear();
			_ignores = ignores;
			force_reset = true;
		}

		_running = true;
		return true;
	}

	bool async_watcher::await_ready() noexcept
	{
		return false;
	}

	void async_watcher::await_suspend(std::coroutine_handle<> h) noexcept
	{
		_semaphore.notify();
		_signal.wait();
		h.resume();
	}

	std::vector<fs::path> async_watcher::await_resume() noexcept
	{
		std::vector<fs::path> paths;
		paths.swap(_watch_result);
		return paths;
	}

	bool async_watcher::overlapped_watch_()
	{
		if (!_running)
		{
			DEBUG_STRING(L"folder_watchor_apc::watch: running=false.\n");
			return false;
		}

		if (!folder_exists_())
		{
			DEBUG_STRING(L"folder_watchor_apc::watch: folder not exists.\n");
			_running = false;
			return false;
		}

		auto result = ReadDirectoryChangesW(
			_folder_handle,
			reinterpret_cast<LPVOID>(_write_buffer.data()),
			static_cast<DWORD>(_write_buffer.size()),
			TRUE,
			gNotifyFilter,
			nullptr,
			&_overlapped,
			async_watcher::completion_routine
		) != FALSE;
		if (!result)
		{
			auto error = GetLastError();
			DEBUG_STRING(L"folder_watchor_apc::watch error: {}\n"sv, error);
		}
		return result;
	}

	bool async_watcher::status_watch_()
	{
		return false;
	}

	bool async_watcher::result_watch_()
	{
		return false;
	}

	bool async_watcher::folder_exists_()
	{
		if (_folder.empty())
		{
			return false;
		}

		if (!fs::exists(_folder))
		{
			return false;
		}

		return true;
	}

	void async_watcher::reset_buffer_(uint32_t size /*= large_buffer_size*/)
	{
		_read_buffer.resize(size);
		_write_buffer.resize(size);
	}

	void async_watcher::notify_information_handle_(DWORD dwNumberOfBytesTransfered)
	{
		if (dwNumberOfBytesTransfered == 0 || dwNumberOfBytesTransfered > _read_buffer.size())
		{
			DEBUG_STRING(L"folder_watchor_base::on_data, maybe need more buffer.\n");
			return;
		}

		auto data = _read_buffer.data();
		auto processed_number = 0;
		while (true)
		{
			auto info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(data);
			parse_notify_information_(info);
			processed_number += info->NextEntryOffset;
			if (info->NextEntryOffset == 0 || processed_number > dwNumberOfBytesTransfered)
			{
				break;
			}
			data += info->NextEntryOffset;
		}
	}

	void async_watcher::parse_notify_information_(PFILE_NOTIFY_INFORMATION info)
	{
		if (info->Action < FILE_ACTION_ADDED || info->Action > FILE_ACTION_RENAMED_NEW_NAME)
		{
			DEBUG_STRING(L"folder_watchor_base::parse_notify_information() failure: action{}.\n", detail::to_str(info->Action));
			return;
		}

		auto filename = std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t));
		auto full_filename = (_folder / filename).lexically_normal();

		if (info->Action == FILE_ACTION_REMOVED || info->Action == FILE_ACTION_RENAMED_OLD_NAME)
		{
			DEBUG_STRING(L"ignore: parse notify {}, name={}\n"sv, info->Action, filename);

			auto file_path = fs::path{ filename }.lexically_normal();
			// TODO: _watch_result.remove(file_path);
			return;
		}

		if (!fs::exists(full_filename))
		{
			DEBUG_STRING(L"ignore: parse notify {}, name={}, not exists.\n"sv, detail::to_str(info->Action), filename);
			return;
		}

		if (info->Action == FILE_ACTION_MODIFIED)
		{
			DEBUG_STRING(L"ignore: parse notify FILE_ACTION_MODIFIED, name={}.\n"sv, filename);
			return;
		}

		if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME)
		{
			DEBUG_STRING(L"update: parse notify {}, name={}\n"sv, detail::to_str(info->Action), filename.c_str());

			if (fs::is_regular_file(full_filename))
			{
				auto file_path = fs::path{ filename }.lexically_normal();
				_watch_result.emplace_back(file_path);
			}
		}
	}

	void async_watcher::completion_routine(DWORD code, DWORD num, LPOVERLAPPED lpov)
	{
		if (!lpov)
		{
			DEBUG_STRING(L"folder_watchor_apc::completion_routine Error self-ptr is null.\n");
			return;
		}

		auto self = reinterpret_cast<async_watcher*>(lpov->hEvent);
		if (!self)
		{
			DEBUG_STRING(L"folder_watchor_apc::completion_routine Error self is null.\n");
			return;
		}

		if (ERROR_SUCCESS != code)
		{
			if (ERROR_INVALID_FUNCTION)
			{
				DEBUG_STRING(L"folder_watchor_apc::completion_routine(): ERROR_INVALID_FUNCTION.\n");
				self->stop();
			}
			else if (code == ERROR_INVALID_PARAMETER)
			{
				DEBUG_STRING(L"folder_watchor_apc::completion_routine(): ERROR_INVALID_PARAMETER.\n");
				self->reset_buffer_(network_max_buffer_size);
				return;
			}
			else if (code == ERROR_NOTIFY_ENUM_DIR)
			{
				DEBUG_STRING(L"folder_watchor_apc::completion_routine(): ERROR_NOTIFY_ENUM_DIR.\n");
			}
			//ERROR_ACCESS_DENIED
			DEBUG_STRING(L"folder_watchor_apc::completion_routine Error: code != ERROR_SUCCESS.\n");
			return;
		}

		self->_write_buffer.swap(self->_read_buffer);
		self->notify_information_handle_(num);
	}
}

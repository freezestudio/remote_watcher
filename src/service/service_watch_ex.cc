#include "service_watch_ex.h"

namespace freeze
{
	folder_watchor_base::folder_watchor_base()
		: overlapped{}
	{
		reset_buffer();
	}

	folder_watchor_base::~folder_watchor_base()
	{
		stop();
	}

	bool folder_watchor_base::set_watch_folder(
		fs::path const& folder,
		std::vector<fs::path> const& ignores /*= {}*/)
	{
		if (folder.empty())
		{
			DEBUG_STRING(L"folder_watchor_base::set_watch_folder(): folder is null.\n");
			return false;
		}

		auto generic_folder = folder.lexically_normal();
		if (!fs::exists(generic_folder))
		{
			DEBUG_STRING(L"folder_watchor_base::set_watch_folder(): folder {} not exists.\n"sv, generic_folder.c_str());
			return false;
		}

		auto force_reset = false;
		if (this->folder != generic_folder)
		{
			auto path_str = generic_folder.c_str();
			this->folder = fs::path{path_str};
			if (!detail::save_latest_folder(path_str))
			{
				DEBUG_STRING(
					L"folder_watchor_base::set_weatch_folder() error: save latest folder: {}.\n"sv,
					generic_folder.c_str());
			}
			if (folder_handle)
			{
				CloseHandle(folder_handle);
				folder_handle = nullptr;
			}
		}

		if (!folder_handle)
		{
			folder_handle = CreateFile(
				this->folder.c_str(),
				FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
				nullptr
			);
			if (INVALID_HANDLE_VALUE == folder_handle)
			{
				DEBUG_STRING(L"folder_watchor_base::set_weatch_folder() error: open folder handle failure.\n");
				folder_handle = nullptr;
				running = false;
				return false;
			}
			force_reset = true;
		}

		if (ignores.size() > 0)
		{
			ignore_folders.clear();
			ignore_folders = ignores;
			force_reset = true;
		}

		watch_tree_ptr = watch_tree_instace(this->folder, ignores, force_reset);

		running = true;
		return true;
	}

	fs::path folder_watchor_base::get_watch_folder() const
	{
		return folder;
	}

	std::vector<fs::path> folder_watchor_base::get_watch_ignores() const
	{
		return ignore_folders;
	}

	bool folder_watchor_base::watch()
	{
		return false;
	}

	void folder_watchor_base::unwatch()
	{
		if (folder_handle)
		{
			CancelIo(folder_handle);
			CloseHandle(folder_handle);
			folder_handle = nullptr;
		}
	}

	void folder_watchor_base::notify_information_handle(DWORD dwNumberOfBytesTransfered)
	{
		if (dwNumberOfBytesTransfered == 0 || dwNumberOfBytesTransfered > read_buffer.size())
		{
			DEBUG_STRING(L"folder_watchor_base::on_data, maybe need more buffer.\n");
			return;
		}

		if (!watch_tree_ptr)
		{
			DEBUG_STRING(L"folder_watchor_base::on_data, watch tree pointer is null.\n");
			return;
		}

		auto data = read_buffer.data();
		auto processed_number = 0;
		while (true)
		{
			auto info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(data);
			parse_notify_information(info);
			processed_number += info->NextEntryOffset;
			if (info->NextEntryOffset == 0 || processed_number > dwNumberOfBytesTransfered)
			{
				break;
			}
			data += info->NextEntryOffset;
		}

		watch_tree_ptr->notify();
	}

	void folder_watchor_base::parse_notify_information(PFILE_NOTIFY_INFORMATION info)
	{
		if (info->Action < FILE_ACTION_ADDED || info->Action > FILE_ACTION_RENAMED_NEW_NAME)
		{
			DEBUG_STRING(L"folder_watchor_base::parse_notify_information() failure: action{}.\n", detail::to_str(info->Action));
			return;
		}

		auto s_filename = detail::to_utf8(info->FileName, info->FileNameLength / sizeof(wchar_t));
		auto filename = detail::to_utf16(s_filename);
		fs::path path_filename = this->folder / filename;
		auto full_filename = path_filename.lexically_normal();

		if (info->Action == FILE_ACTION_REMOVED || info->Action == FILE_ACTION_RENAMED_OLD_NAME)
		{
			DEBUG_STRING(L"ignore: parse notify {}, name={}\n"sv, info->Action, filename);

			auto file_path = fs::path{ filename }.lexically_normal();
			watch_tree_ptr->remove(file_path);
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
				watch_tree_ptr->add(file_path);
			}
		}
	}

	bool folder_watchor_base::folder_exists() const
	{
		if (folder.empty())
		{
			return false;
		}

		if (!fs::exists(folder))
		{
			return false;
		}

		return true;
	}

	void folder_watchor_base::reset_buffer(uint32_t size /*= large_buffer_size*/)
	{
		read_buffer.resize(size);
		write_buffer.resize(size);
	}
}

namespace freeze
{
	folder_watchor_apc::folder_watchor_apc()
		//: thread(folder_watchor_apc::loop_thread, this)
		: signal{}
	{
		overlapped.hEvent = this;
	}

	folder_watchor_apc::~folder_watchor_apc()
	{
		stop();
	}

	bool folder_watchor_apc::watch()
	{
		if (!running)
		{
			DEBUG_STRING(L"folder_watchor_apc::watch: can not run.\n");
			return false;
		}

		if (!folder_exists())
		{
			DEBUG_STRING(L"folder_watchor_apc::watch: folder not exists, can not run.\n");
			running = false;
			return false;
		}

		auto result = ReadDirectoryChangesW(
			folder_handle,
			reinterpret_cast<LPVOID>(write_buffer.data()),
			static_cast<DWORD>(write_buffer.size()),
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			folder_watchor_apc::completion_routine
		) != FALSE;
		if (!result)
		{
			auto error = GetLastError();
			DEBUG_STRING(L"folder_watchor_apc::watch error: {}\n"sv, error);
		}
		return result;
	}

	void folder_watchor_apc::start()
	{
		thread = std::thread(folder_watchor_apc::loop_thread, this);
		DEBUG_STRING(L"folder_watchor_apc::start(): wait thread run ...\n");

		signal.wait();
		auto ret = QueueUserAPC([](ULONG_PTR instance)
			{
				auto self = reinterpret_cast<folder_watchor_apc*>(instance);
				if (self)
				{
					self->watch();
				}
				else
				{
					DEBUG_STRING(L"folder_watchor_apc::start() error: install is null.\n");
				}
			}, thread.native_handle(), (ULONG_PTR)(this));
		if (!ret)
		{
			auto err = GetLastError();
			DEBUG_STRING(L"folder_watchor_apc::start() apc error: {}\n"sv, err);
		}
	}

	void folder_watchor_apc::stop()
	{
		running = false;
		auto ret = QueueUserAPC([](ULONG_PTR) {}, thread.native_handle(), (ULONG_PTR)(this));
		if (!ret)
		{
			auto err = GetLastError();
			DEBUG_STRING(L"folder_watchor_apc::stop() apc error: {}\n"sv, err);
		}
		if (thread.joinable())
		{
			thread.join();
		}
		else
		{
			thread.detach();
		}
	}

	void folder_watchor_apc::loop_thread(void* instance)
	{
		auto self = reinterpret_cast<folder_watchor_apc*>(instance);
		if (!self)
		{
			DEBUG_STRING(L"Error SleepEx result, Self is null.\n");
			return;
		}
		self->signal.notify();

		while (true)
		{
			DEBUG_STRING(L"folder_watchor_apc::loop_thread(): thread run and sleep.\n");
			auto result = SleepEx(INFINITE, TRUE);
			if (WAIT_IO_COMPLETION != result)
			{
				DEBUG_STRING(L"Error SleepEx result not WAIT_IO_COMPLETION!.\n");
				//break;
			}
			if (!self->running)
			{
				DEBUG_STRING(L"Error SleepEx result, Self::running is false.\n");
				break;
			}
		}
	}

	void folder_watchor_apc::completion_routine(DWORD code, DWORD num, LPOVERLAPPED lpov)
	{
		if (!lpov)
		{
			DEBUG_STRING(L"folder_watchor_apc::completion_routine Error self-ptr is null.\n");
			return;
		}

		auto self = reinterpret_cast<folder_watchor_apc*>(lpov->hEvent);
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
				self->reset_buffer(network_max_buffer_size);
				self->watch();
			}
			else if (code == ERROR_NOTIFY_ENUM_DIR)
			{
				DEBUG_STRING(L"folder_watchor_apc::completion_routine(): ERROR_NOTIFY_ENUM_DIR.\n");
				self->watch();
			}
			//ERROR_ACCESS_DENIED
			DEBUG_STRING(L"folder_watchor_apc::completion_routine Error: code != ERROR_SUCCESS.\n");
			return;
		}

		self->write_buffer.swap(self->read_buffer);
		self->watch();
		self->notify_information_handle(num);
	}
}

namespace freeze
{
	folder_watchor_status::folder_watchor_status()
		: io_port_handle{ nullptr }
	{

	}

	folder_watchor_status::~folder_watchor_status()
	{
		stop();
	}

	bool folder_watchor_status::watch()
	{
		if (!running)
		{
			return false;
		}

		if (!folder_exists())
		{
			running = false;
			return false;
		}

		auto result = ReadDirectoryChangesW(
			folder_handle,
			reinterpret_cast<LPVOID>(write_buffer.data()),
			static_cast<DWORD>(write_buffer.size()),
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			nullptr
		) != FALSE;
		if (!result)
		{
			auto error = GetLastError();
			DEBUG_STRING(L"folder_watchor_status::watch error: {}\n"sv, error);
		}
		// block until completion ...
		if (result)
		{
			if (!io_port_handle)
			{
				io_port_handle = CreateIoCompletionPort(folder_handle, nullptr, 0, 0);
				if (!io_port_handle)
				{
					running = false;
					return false;
				}
			}
			DWORD bytes_transfer = 0;
			ULONG_PTR key;
			LPOVERLAPPED lpov = &overlapped;
			result = GetQueuedCompletionStatus(io_port_handle, &bytes_transfer, &key, &lpov, INFINITE);
			if (result)
			{
				write_buffer.swap(read_buffer);
				this->notify_information_handle(bytes_transfer);
			}
		}
		if (!result)
		{
			running = false;
		}
		return result;
	}

	void folder_watchor_status::start()
	{
		while (running)
		{
			watch();
		}
	}

	void folder_watchor_status::stop()
	{
		running = false;
		if (io_port_handle)
		{
			CloseHandle(io_port_handle);
			io_port_handle = nullptr;
		}
	}
}

namespace freeze
{
	folder_watchor_result::folder_watchor_result()
	{
		overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}

	folder_watchor_result::~folder_watchor_result()
	{
		stop();
	}

	bool folder_watchor_result::watch()
	{
		if (!running)
		{
			return false;
		}

		if (!folder_exists())
		{
			running = false;
			return false;
		}

		auto result = ReadDirectoryChangesW(
			folder_handle,
			reinterpret_cast<LPVOID>(write_buffer.data()),
			static_cast<DWORD>(write_buffer.size()),
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			nullptr
		) != FALSE;
		if (!result)
		{
			auto error = GetLastError();
			DEBUG_STRING(L"folder_watchor_result::watch error: {}\n"sv, error);
		}

		// block waiting ...
		if (result)
		{
			DWORD bytes_transfer = 0;
			result = GetOverlappedResult(folder_handle, &overlapped, &bytes_transfer, TRUE);
			if (result)
			{
				ResetEvent(overlapped.hEvent);
				write_buffer.swap(read_buffer);
				this->notify_information_handle(bytes_transfer);
			}
		}
		if (!result)
		{
			running = false;
		}
		return result;
	}

	void folder_watchor_result::start()
	{
		while (running)
		{
			watch();
		}
	}

	void folder_watchor_result::stop()
	{
		running = false;
		if (overlapped.hEvent)
		{
			CloseHandle(overlapped.hEvent);
		}
	}
}

namespace freeze
{
	watcher_win::watcher_win(folder_watchor_base& watchor)
		: watchor{ watchor }
	{

	}

	void watcher_win::start()
	{
		if (!watchor.set_watch_folder(folder, ignore_folders))
		{
			DEBUG_STRING(L"watcher_win::start() error: underline watchor set watch folder failure.\n");
			return;
		}
		watchor.start();
	}

	void watcher_win::stop()
	{
		watchor.stop();
	}

	void watcher_win::set_watch_folder(fs::path const& folder)
	{
		this->folder = folder;
	}

	void watcher_win::set_ignore_folders(std::vector<fs::path> const& ignores)
	{
		this->ignore_folders = ignores;
	}
}

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
			return false;
		}

		auto generic_folder = folder.lexically_normal();
		if (!fs::exists(generic_folder))
		{
			return false;
		}

		if (this->folder != generic_folder)
		{
			this->folder = generic_folder;
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
				folder_handle = nullptr;
				running = false;
				return false;
			}
		}

		if (ignores.size() > 0)
		{
			ignore_folders.clear();
			ignore_folders = ignores;
		}

		watch_tree_ptr = watch_tree_instace(this->folder, ignores);

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
			OutputDebugString(L"folder_watchor_base::on_data, maybe need more buffer.\n");
			return;
		}

		if (!watch_tree_ptr)
		{
			OutputDebugString(L"folder_watchor_base::on_data, watch tree pointer is null.\n");
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
		if (info->Action<FILE_ACTION_ADDED || info->Action>FILE_ACTION_RENAMED_NEW_NAME)
		{
			return;
		}

		auto filename = std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t));
		auto full_filename = (folder / filename).lexically_normal();

		if (info->Action == FILE_ACTION_REMOVED || info->Action == FILE_ACTION_RENAMED_OLD_NAME)
		{
			auto msg = std::format(L"ignore: parse notify {}, name={}\n"sv, info->Action, filename);
			OutputDebugString(msg.c_str());

			auto file_path = fs::path{ filename }.lexically_normal();
			watch_tree_ptr->remove(file_path);
			return;
		}

		if (!fs::exists(full_filename))
		{
			auto msg = std::format(L"ignore: parse notify {}, name={}, not exists.\n"sv, info->Action, filename);
			OutputDebugString(msg.c_str());
			return;
		}

		if (info->Action == FILE_ACTION_MODIFIED)
		{
			auto msg = std::format(L"ignore: parse notify FILE_ACTION_MODIFIED, name={}.\n"sv, filename);
			OutputDebugString(msg.c_str());
			return;
		}

		if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME)
		{
			auto msg = std::format(L"update: parse notify {}, name={}\n"sv, info->Action, filename.c_str());
			OutputDebugString(msg.c_str());

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
			OutputDebugString(L"folder_watchor_apc::watch: running=false.\n");
			return false;
		}

		if (!folder_exists())
		{
			OutputDebugString(L"folder_watchor_apc::watch: folder not exists.\n");
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
			auto msg = std::format(L"folder_watchor_apc::watch error: {}\n"sv, error);
			OutputDebugString(msg.c_str());
		}
		return result;
	}

	void folder_watchor_apc::start()
	{
		thread = std::thread(folder_watchor_apc::loop_thread, this);
		signal.wait();
		QueueUserAPC([](ULONG_PTR instance)
			{
				auto self = reinterpret_cast<folder_watchor_apc*>(instance);
				if (self)
				{
					self->watch();
				}
			}, thread.native_handle(), (ULONG_PTR)(this));
		// if (thread.joinable())
		// {
		// 	thread.join();
		// }
	}

	void folder_watchor_apc::stop()
	{
		running = false;
		QueueUserAPC([](ULONG_PTR) {}, thread.native_handle(), (ULONG_PTR)(this));
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
			OutputDebugString(L"Error SleepEx result, Self is null.\n");
			return;
		}
		self->signal.notify();
		while (true)
		{
			auto result = SleepEx(INFINITE, TRUE);
			if (WAIT_IO_COMPLETION != result)
			{
				OutputDebugString(L"Error SleepEx result not WAIT_IO_COMPLETION!.\n");
				break;
			}
			if (!self->running)
			{
				OutputDebugString(L"Error SleepEx result, Self::running is false.\n");
				break;
			}
		}
	}

	void folder_watchor_apc::completion_routine(DWORD code, DWORD num, LPOVERLAPPED lpov)
	{
		if (!lpov)
		{
			OutputDebugString(L"folder_watchor_apc::completion_routine Error self-ptr is null.\n");
			return;
		}

		auto self = reinterpret_cast<folder_watchor_apc*>(lpov->hEvent);
		if (!self)
		{
			OutputDebugString(L"folder_watchor_apc::completion_routine Error self is null.\n");
			return;
		}

		if (ERROR_SUCCESS != code)
		{
			if (ERROR_INVALID_FUNCTION)
			{
				self->stop();
			}
			else if (code == ERROR_INVALID_PARAMETER)
			{
				self->reset_buffer(network_max_buffer_size);
				self->watch();
			}
			else if(code == ERROR_NOTIFY_ENUM_DIR)
			{
				self->watch();
			}
			//ERROR_ACCESS_DENIED
			OutputDebugString(L"folder_watchor_apc::completion_routine Error: code != ERROR_SUCCESS.\n");
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
			auto msg = std::format(L"folder_watchor_status::watch error: {}\n"sv, error);
			OutputDebugString(msg.c_str());
		}
		// block until completion ...
		if (result)
		{
			auto port = CreateIoCompletionPort(folder_handle, nullptr, 0, 0);
			if (!port)
			{
				return false;
			}
			DWORD bytes_transfer = 0;
			ULONG_PTR key;
			LPOVERLAPPED lpov = &overlapped;
			result = GetQueuedCompletionStatus(port, &bytes_transfer, &key, &lpov, INFINITE);
			if(result)
			{
				write_buffer.swap(read_buffer);
				this-> notify_information_handle(bytes_transfer);
			}
		}
		return result;
	}

	void folder_watchor_status::start()
	{
		while(running)
		{
			watch();
		}
	}

	void folder_watchor_status::stop()
	{
		running = false;
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
			auto msg = std::format(L"folder_watchor_result::watch error: {}\n"sv, error);
			OutputDebugString(msg.c_str());
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

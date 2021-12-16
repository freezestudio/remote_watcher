#include "service_watch_ex.h"

namespace freeze
{
	watch_tree::watch_tree()
	{

	}

	watch_tree::watch_tree(fs::path const& folder, std::vector<fs::path> const& ignore_folders)
		: folder{ folder }
		, ignore_folders{ ignore_folders }
	{

	}

	void watch_tree::add(fs::path const& file)
	{
		std::lock_guard<std::mutex> lock(mutex);
		files.emplace(file);
	}

	void watch_tree::remove(fs::path const& file)
	{
		auto _file = maybe_include(file);
		if (!_file.has_value() || _file.value().empty())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex);
		files.erase(file);
	}

	void watch_tree::modify(fs::path const& file)
	{
		//auto _file = maybe_include(file);
		//if (!_file.has_value() || _file.value().empty())
		//{
		//	return;
		//}
		//std::lock_guard<std::mutex> lock(mutex);
	}

	std::optional<fs::path> watch_tree::maybe_include(fs::path const& file)
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& item : files)
		{
			if (item == file)
			{
				return item;
			}
		}
		return std::nullopt;
	}
}

namespace freeze
{
	//
	static std::unordered_map<
		fs::path,
		std::weak_ptr<watch_tree>,
		path_hasher,
		path_equal_to
	> path_watch_trees;

	static auto watch_tree_instace(fs::path const& folder, std::vector<fs::path>const& ignores = {})
	{
		auto generic_folder = folder.lexically_normal();
		std::shared_ptr<watch_tree> tree = std::make_shared<watch_tree>(generic_folder, ignores);
		auto pair = path_watch_trees.try_emplace(generic_folder, tree);
		return pair.first->second.lock();
	}
}

namespace freeze
{
	folder_watchor_base::folder_watchor_base()
		: overlapped{}
	{
		reset_buffer();
	}

	folder_watchor_base::~folder_watchor_base()
	{

	}

	/*virtual*/
	bool folder_watchor_base::set_watch_folder(
		fs::path const& folder, 
		std::vector<fs::path> const& ignores /*= {}*/) /*override*/
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

	/*virtual*/
	fs::path folder_watchor_base::get_watch_folder() const /*override*/
	{
		return folder;
	}

	/*virtual*/
	std::vector<fs::path> folder_watchor_base::get_watch_ignores() const /*override*/
	{
		return ignore_folders;
	}


	/*virtual*/
	bool folder_watchor_base::watch(uint32_t size /*= large_buffer_size*/) /*override*/
	{
		return false;
	}

	/*virtual*/
	void folder_watchor_base::unwatch() /*override*/
	{

	}


	void folder_watchor_base::notify_information_handle()
	{
		auto data = read_buffer.get();
		for (auto info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(data);;)
		{
			parse_notify_information(info);
			if (info->NextEntryOffset == 0)
			{
				break;
			}
			info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(data + info->NextEntryOffset);
		}
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
		read_buffer = std::make_unique<std::byte[]>(size);
		write_buffer = std::make_unique<std::byte[]>(size);
	}
}

namespace freeze
{
	folder_watchor_apc::folder_watchor_apc()
		: thread(folder_watchor_apc::loop_thread, this)
	{
		overlapped.hEvent = this;
	}

	folder_watchor_apc::~folder_watchor_apc()
	{
		unwatch();
	}

	/*virtual*/
	bool folder_watchor_apc::watch(uint32_t size /*= large_buffer_size*/) /*override*/
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
			write_buffer.get(),
			size,
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			folder_watchor_apc::completion_routine
		) > 0;
		return result;
	}

	/*virtual*/
	void folder_watchor_apc::unwatch() /*override*/
	{
		stop();
		if (folder_handle)
		{
			CloseHandle(folder_handle);
		}
	}

	void folder_watchor_apc::stop()
	{
		running = false;
		QueueUserAPC([](ULONG_PTR) {}, thread.native_handle(), (ULONG_PTR)(this));
		if (thread.joinable())
		{
			thread.join();
		}
	}

	/*static*/
	void folder_watchor_apc::loop_thread(void* instance)
	{
		while (true)
		{
			auto result = SleepEx(INFINITE, TRUE);
			if (WAIT_IO_COMPLETION != result)
			{
				OutputDebugString(L"Error SleepEx result not WAIT_IO_COMPLETION!.\n");
				break;
			}
			auto self = reinterpret_cast<folder_watchor_apc*>(instance);
			if (!self)
			{
				OutputDebugString(L"Error SleepEx result, Self is null.\n");
				break;
			}
			if (!self->running)
			{
				OutputDebugString(L"Error SleepEx result, Self::running is false.\n");
				break;
			}
		}
	}

	/*static*/
	void folder_watchor_apc::completion_routine(DWORD code, DWORD num, LPOVERLAPPED lpov)
	{
		if (!lpov)
		{
			return;
		}

		auto self = reinterpret_cast<folder_watchor_apc*>(lpov->hEvent);
		if (!self)
		{
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
				self->watch(network_max_buffer_size);
			}
			return;
		}

		self->write_buffer.swap(self->read_buffer);
		self->watch();
		self->notify_information_handle();
	}
}

namespace freeze
{
	folder_watchor_status::folder_watchor_status()
	{

	}

	folder_watchor_status::~folder_watchor_status()
	{

	}

	/*virtual*/
	bool folder_watchor_status::watch(uint32_t size /*= large_buffer_size*/) /*override*/
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
			write_buffer.get(),
			size,
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			nullptr
		) > 0;

		// block until completion
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
		}

		return result;
	}

	/*virtual*/
	void folder_watchor_status::unwatch() /*override*/
	{

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
		if (overlapped.hEvent)
		{
			CloseHandle(overlapped.hEvent);
		}
	}

	/*virtual*/
	bool folder_watchor_result::watch(uint32_t size /*= large_buffer_size*/) /*override*/
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
			write_buffer.get(),
			size,
			TRUE,
			gNotifyFilter,
			nullptr,
			&overlapped,
			nullptr
		) > 0;

		if (result)
		{
			DWORD bytes_transfer = 0;
			result = GetOverlappedResult(folder_handle, &overlapped, &bytes_transfer, TRUE);
			if (result)
			{
				ResetEvent(overlapped.hEvent);
				//this->notify_information_handle();
			}
		}
		return result;
	}

	/*virtual*/
	void folder_watchor_result::unwatch() /*override*/
	{

	}
}

namespace freeze
{
	watcher_win::watcher_win(watchable& underline)
		: watchor{ underline }
		, signal{}
	{

	}

	/*virtual*/
	void watcher_win::start() /*override*/
	{
		if (!watchor.set_watch_folder(folder, ignore_folders))
		{
			return;
		}

		thread = std::thread([this]()
			{
				watchor.watch(large_buffer_size);
				signal.notify();
			});
		signal.wait();
		if (thread.joinable())
		{
			thread.join();
		}
	}

	void watcher_win::set_watch_folder(fs::path const& folder)
	{
		this->folder = folder;
	}

	void watcher_win::set_ignore_folders(std::vector<fs::path> const& paths)
	{
		this->ignore_folders = paths;
	}

	//watcher_task watcher_win::fill_watch_tree()
	//{
	//}
}

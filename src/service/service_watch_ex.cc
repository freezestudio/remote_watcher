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
		if (_file.has_value() && !_file.value().empty())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex);
		files.erase(file);
	}

	void watch_tree::modify(fs::path const& file)
	{
		//auto _file = maybe_include(file);
		//if (_file.has_value() && !_file.value().empty())
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
	folder_watchor_apc::folder_watchor_apc()
		: thread(folder_watchor_apc::loop_thread, this)
		, signal{}
		, overlapped{}
	{
		overlapped.hEvent = this;
	}

	/*virtual*/
	fs::path folder_watchor_apc::get_watch_folder() const /*override*/
	{
		return {};
	}

	/*virtual*/
	std::vector<fs::path> folder_watchor_apc::get_watch_filter() const /*override*/
	{
		return {};
	}

	/*virtual*/
	bool folder_watchor_apc::watch_loop(uint32_t size /*= large_buffer_size*/) /*override*/
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

	void folder_watchor_apc::stop()
	{
		running = false;
		QueueUserAPC([](ULONG_PTR) {}, thread.native_handle(), (ULONG_PTR)(this));
		if (thread.joinable())
		{
			thread.join();
		}
	}

	bool folder_watchor_apc::set_watch_folder(fs::path const& folder, std::vector<fs::path> const& ignores /*= {}*/)
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
			this->folder = folder;
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

		watch_tree_ptr = watch_tree_instace(this->folder, ignores);

		running = true;
		return true;
	}

	/*static*/
	void folder_watchor_apc::loop_thread(void* instance)
	{
		while (true)
		{
			auto result = SleepEx(INFINITE, TRUE);
			if (WAIT_IO_COMPLETION != result)
			{
				break;
			}
			auto self = reinterpret_cast<folder_watchor_apc*>(instance);
			if (!self)
			{
				break;
			}
			if (!self->running)
			{
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
				self->watch_loop(network_max_buffer_size);
			}
			return;
		}

		self->write_buffer.swap(self->read_buffer);
		self->watch_loop();
		self->notify_information_handle();
	}

	void folder_watchor_apc::notify_information_handle()
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

	void folder_watchor_apc::parse_notify_information(PFILE_NOTIFY_INFORMATION info)
	{
		auto filename = std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t));
		auto full_filename = (folder / filename).lexically_normal();

		if (info->Action == FILE_ACTION_REMOVED || info->Action == FILE_ACTION_RENAMED_OLD_NAME)
		{
			auto msg = std::format(L"ignore: parse notify{}, name={}\n"sv, info->Action, filename.c_str());
			OutputDebugString(msg.c_str());

			auto file_path = fs::path{ filename }.lexically_normal();
			watch_tree_ptr->remove(file_path);
			return;
		}

		if (!fs::exists(full_filename))
		{
			auto msg = std::format(L"ignore: parse notify{}, name={}, not exists.\n"sv, info->Action, filename.c_str());
			OutputDebugString(msg.c_str());
			return;
		}

		if (info->Action == FILE_ACTION_MODIFIED)
		{
			auto msg = std::format(L"ignore: parse notify{FILE_ACTION_MODIFIED}, name={}.\n"sv, filename.c_str());
			OutputDebugString(msg.c_str());
			return;
		}

		if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME)
		{
			auto msg = std::format(L"update: parse notify{}, name={}\n"sv, info->Action, filename.c_str());
			OutputDebugString(msg.c_str());

			auto file_path = fs::path{ filename }.lexically_normal();
			watch_tree_ptr->add(file_path);
		}
	}

	bool folder_watchor_apc::folder_exists() const
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

	void folder_watchor_apc::reset_buffer(uint32_t size /*= large_buffer_size*/)
	{
		read_buffer = std::make_unique<std::byte[]>(size);
		write_buffer = std::make_unique<std::byte[]>(size);
	}
}

namespace freeze
{
	folder_watchor_query::folder_watchor_query()
	{

	}

	/*virtual*/
	fs::path folder_watchor_query::get_watch_folder() const /*override*/
	{
		return {};
	}

	/*virtual*/
	std::vector<fs::path> folder_watchor_query::get_watch_filter() const /*override*/
	{
		return {};
	}

	/*virtual*/
	bool folder_watchor_query::watch_loop(uint32_t size /*= large_buffer_size*/) /*override*/
	{
		return false;
	}
}

namespace freeze
{
	folder_watchor_result::folder_watchor_result()
	{

	}

	/*virtual*/
	fs::path folder_watchor_result::get_watch_folder() const /*override*/
	{
		return {};
	}

	/*virtual*/
	std::vector<fs::path> folder_watchor_result::get_watch_filter() const /*override*/
	{
		return {};
	}

	/*virtual*/
	bool folder_watchor_result::watch_loop(uint32_t size /*= large_buffer_size*/) /*override*/
	{
		return false;
	}
}

namespace freeze
{
	watcher_win::watcher_win(watchable& underline)
		: watchor{ underline }
	{

	}

	/*virtual*/
	void watcher_win::start() /*override*/
	{

	}

	void watcher_win::set_watch_folder()
	{

	}

	void watcher_win::set_ignore_folders(std::vector<fs::path> const& paths)
	{

	}

	//watcher_task watcher_win::fill_watch_tree()
	//{
	//}
}

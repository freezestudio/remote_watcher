#include "service_watch_tree.h"


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
		// if exists, replace.
	}

	void watch_tree::clear()
	{
		std::lock_guard<std::mutex> lock(mutex);
		files.clear();
	}

	void watch_tree::notify()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (files.size() > 0)
		{
			global_reason_signal.notify_reason(sync_reason_send_payload);
		}
	}

	std::vector<fs::path> watch_tree::get_all()
	{
		std::lock_guard<std::mutex> lock(mutex);
		std::vector<fs::path> vec_files;
		std::copy_if(std::cbegin(files), std::cend(files), std::back_inserter(vec_files), [this](auto&& f)
			{
				return fs::is_regular_file((folder / f));
			});
		return vec_files;
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
	// global root folder trees.
	static std::unordered_map<
		fs::path,
		std::weak_ptr<watch_tree>,
		path_hasher,
		path_equal_to
	> path_watch_trees;
	static std::shared_ptr<watch_tree> _g_watch_tree_instance = nullptr;
	static std::mutex _g_mutex;
	std::shared_ptr<watch_tree> watch_tree_instace(fs::path const& folder, std::vector<fs::path>const& ignores /*= {}*/, bool reset /*= false*/)
	{
		std::lock_guard<std::mutex> lock(_g_mutex);
		if (!_g_watch_tree_instance || reset)
		{
			auto generic_folder = folder.lexically_normal();
			std::shared_ptr<watch_tree> tree = std::make_shared<watch_tree>(generic_folder, ignores);
			auto pair = path_watch_trees.try_emplace(generic_folder, tree);
			_g_watch_tree_instance = pair.first->second.lock();
		}
		return _g_watch_tree_instance;
	}
}

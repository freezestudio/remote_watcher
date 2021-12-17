#ifndef SERVICE_WATCH_TREE_H
#define SERVICE_WATCH_TREE_H

#include "common_dep.h"
#include "service_dep.h"

namespace freeze
{
	struct path_hasher
	{
		[[nodiscard]] size_t operator()(fs::path const& rhs) const noexcept
		{
			return fs::hash_value(rhs);
		}
	};

	struct path_equal_to
	{
		bool operator()(const fs::path& lhs, const fs::path& rhs) const noexcept
		{
			return lhs == rhs;
		}
	};
}

namespace freeze
{
	class watch_tree
	{
	public:
		watch_tree();
		watch_tree(fs::path const& folder, std::vector<fs::path> const& ignore_folders = {});

	public:
		void add(fs::path const& file);
		void remove(fs::path const& file);
		void modify(fs::path const& file);
		void clear();

	public:
		void notify();
		std::vector<fs::path> get_all();

	public:
		fs::path folder;
		std::vector<fs::path> ignore_folders;

	private:
		std::optional<fs::path> maybe_include(fs::path const& file);

	private:
		std::unordered_set<fs::path, path_hasher, path_equal_to> files;
		std::mutex mutex;
	};

	std::shared_ptr<watch_tree> watch_tree_instace(
		fs::path const& folder, std::vector<fs::path>const& ignores = {}, bool reset = false);
}

#endif

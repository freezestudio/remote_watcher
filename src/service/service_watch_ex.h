#ifndef SERVICE_WATCH_EX_H
#define SERVICE_WATCH_EX_H

#include "common_dep.h"

#include "service_dep.h"
#include "service_utils.h"
#include "service_async.hpp"

constexpr auto network_max_buffer_size = 64 * 1024UL;
constexpr auto large_buffer_size = 512 * 1024UL;

namespace freeze
{
	class watchable;
	class watcher_base;
	class watcher_win;
	class folder_watchor;
	class folder_watchor_base;
	class watch_tree;
}

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
	class watchable
	{
	public:
		virtual bool set_watch_folder(fs::path const&, std::vector<fs::path> const& = {}) = 0;
		virtual fs::path get_watch_folder() const = 0;
		virtual std::vector<fs::path> get_watch_ignores() const = 0;

	public:
		virtual bool watch(uint32_t=0) = 0;
		virtual void unwatch() = 0;

	public:
		virtual ~watchable() {}
	};

	class watcher_base
	{
	public:
		virtual void start() = 0;

	public:
		virtual ~watcher_base() {}
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

	public:
		fs::path folder;
		std::vector<fs::path> ignore_folders;

	private:
		std::optional<fs::path> maybe_include(fs::path const& file);

	private:
		std::unordered_set<fs::path, path_hasher, path_equal_to> files;
		std::mutex mutex;
	};
}

namespace freeze
{
	class folder_watchor_base : public watchable
	{
	public:
		folder_watchor_base();
		virtual ~folder_watchor_base();

	public:
		virtual bool set_watch_folder(fs::path const& folder, std::vector<fs::path> const& ignores = {}) override;
		virtual fs::path get_watch_folder() const override;
		virtual std::vector<fs::path> get_watch_ignores() const override;

	public:
		virtual bool watch(uint32_t = large_buffer_size) override;
		virtual void unwatch() override;

	public:
		bool folder_exists() const;
		void reset_buffer(uint32_t = large_buffer_size);
		void notify_information_handle();
		void parse_notify_information(PFILE_NOTIFY_INFORMATION);

	protected:
		OVERLAPPED overlapped;
		fs::path folder;
		std::vector<fs::path> ignore_folders;
		std::unique_ptr<std::byte[]> read_buffer;
		std::unique_ptr<std::byte[]> write_buffer;

		bool running = false;
		HANDLE folder_handle{ nullptr };
		std::shared_ptr<watch_tree> watch_tree_ptr{ nullptr };
	};

	class folder_watchor_apc : public folder_watchor_base
	{
	public:
		folder_watchor_apc();
		~folder_watchor_apc();

	public:
		virtual bool watch(uint32_t = large_buffer_size) override;
		virtual void unwatch() override;

	public:
		void stop();

	private:
		static void loop_thread(void*);
		static void completion_routine(DWORD, DWORD, LPOVERLAPPED);

	private:
		std::thread thread;
	};

	class folder_watchor_status : public folder_watchor_base
	{
	public:
		folder_watchor_status();
		~folder_watchor_status();

	public:
		virtual bool watch(uint32_t = large_buffer_size) override;
		virtual void unwatch() override;

	private:

	};

	class folder_watchor_result : public folder_watchor_base
	{
	public:
		folder_watchor_result();
		~folder_watchor_result();

	public:
		virtual bool watch(uint32_t = large_buffer_size) override;
		virtual void unwatch() override;
	};

	class watcher_win : public watcher_base
	{
	public:
		watcher_win(watchable& underline);

	public:
		virtual void start() override;

	public:
		void set_watch_folder(fs::path const& folder);
		void set_ignore_folders(std::vector<fs::path> const& paths);

	public:
		//watcher_task fill_watch_tree();

	private:
		watchable& watchor;
		atomic_sync signal;
		std::thread thread;
		fs::path folder;
		std::vector<fs::path> ignore_folders;
	};
}

#endif

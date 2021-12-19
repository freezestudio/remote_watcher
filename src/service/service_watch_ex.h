#ifndef SERVICE_WATCH_EX_H
#define SERVICE_WATCH_EX_H

#include "common_dep.h"

#include "service_dep.h"
#include "service_utils.h"
#include "service_async.hpp"
#include "service_watch_tree.h"

namespace freeze
{
	class watchable;
	class watcher_base;
	class watcher_win;
	class folder_watchor;
	class folder_watchor_base;
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
		virtual bool watch() = 0;
		virtual void unwatch() = 0;

	public:
		virtual ~watchable() {}
	};

	class watcher_base
	{
	public:
		virtual void start() = 0;
		virtual void stop() = 0;

	public:
		virtual ~watcher_base() {}
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
		virtual bool watch() override;
		virtual void unwatch() override;

	public:
		virtual void start() {}
		virtual void stop() {}

	public:
		bool folder_exists() const;
		void reset_buffer(uint32_t = large_buffer_size);
		void notify_information_handle(DWORD);
		void parse_notify_information(PFILE_NOTIFY_INFORMATION);

	protected:
		OVERLAPPED overlapped;
		fs::path folder;
		std::vector<fs::path> ignore_folders;
		std::vector<std::byte> read_buffer;
		std::vector<std::byte> write_buffer;

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
		virtual bool watch() override;

	public:
		void start() override;
		void stop() override;

	private:
		static void loop_thread(void*);
		static void completion_routine(DWORD, DWORD, LPOVERLAPPED);

	private:
		std::thread thread;
		atomic_sync signal;
	};

	class folder_watchor_status : public folder_watchor_base
	{
	public:
		folder_watchor_status();
		~folder_watchor_status();

	public:
		virtual bool watch() override;

	public:
		void start() override;
		void stop() override;

	private:
		HANDLE io_port_handle;
	};

	class folder_watchor_result : public folder_watchor_base
	{
	public:
		folder_watchor_result();
		~folder_watchor_result();

	public:
		virtual bool watch() override;

	public:
		void start() override;
		void stop() override;
	};
}

namespace freeze
{
	class watcher_win : public watcher_base
	{
	public:
		watcher_win(folder_watchor_base&);

	public:
		virtual void start() override;
		virtual void stop() override;

	public:
		void set_watch_folder(fs::path const&);
		void set_ignore_folders(std::vector<fs::path> const&);

	private:
		folder_watchor_base& watchor;
		fs::path folder;
		std::vector<fs::path> ignore_folders;
	};
}

#endif

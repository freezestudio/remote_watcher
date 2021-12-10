//
// service dependence head files
//

#include <thread>
#include <vector>
#include <mutex>

// out namespaces
using namespace std::literals;
namespace fs = std::filesystem;

template<typename T>
concept Stringify = requires (T t) {
	t.c_str();
} || requires(T t) {
	t.data();
};


// ignore FILE_NOTIFY_CHANGE_SECURITY
constexpr auto gNotifyFilter =
FILE_NOTIFY_CHANGE_FILE_NAME |
FILE_NOTIFY_CHANGE_DIR_NAME |
FILE_NOTIFY_CHANGE_ATTRIBUTES |
FILE_NOTIFY_CHANGE_SIZE |
FILE_NOTIFY_CHANGE_LAST_WRITE |
FILE_NOTIFY_CHANGE_LAST_ACCESS |
FILE_NOTIFY_CHANGE_CREATION;

// accept all actions
constexpr auto gNotifyAction =
FILE_ACTION_ADDED |
FILE_ACTION_REMOVED |
FILE_ACTION_MODIFIED |
FILE_ACTION_RENAMED_OLD_NAME |
FILE_ACTION_RENAMED_NEW_NAME;

namespace freeze::detail
{
	inline fs::path to_normal(fs::path const& path)
	{
		return path.lexically_normal();
	}

	inline bool check_exist(fs::path const& path)
	{
		return !path.empty() && fs::exists(path);
	}

	inline bool normal_check_exists(fs::path const& path)
	{
		return check_exist(to_normal(path));
	}
}

namespace freeze
{
	enum class response_type
	{
		result, // GetOverlappedResult
		status, // GetQueuedCompletionStatus
		overlapped,
	};

	class watchor
	{
	public:
		watchor();
		~watchor();

		watchor(watchor const&) = delete;
		watchor& operator=(watchor const&) = delete;

		watchor(watchor&&) noexcept;
		watchor& operator=(watchor&&) noexcept;

	public:
		template<Stringify S>
		bool set_folder(S&& folder);

		void reset_buffer(DWORD = 1024 * 1024);
		bool start(response_type = response_type::overlapped);
		void stop();

	public:
		void notify_information(DWORD dwNumberOfBytesTransfered);
		void parse_information(PFILE_NOTIFY_INFORMATION info);

	public:
		static void OverlappedCompletionRoutine(
			DWORD dwErrorCode,
			DWORD dwNumberOfBytesTransfered,
			LPOVERLAPPED lpOverlapped);

	public:
		static void OverlappedCompletionCallback(ULONG_PTR parameter);
		static void OverlappedCompletionResult(LPOVERLAPPED, LPVOID lpContext);
		static void OverlappedCompletionStatus(LPOVERLAPPED, LPVOID lpContext);

	private:
		void _move(watchor&&);
		decltype(auto) _start_thread();
		void _stop_thread();

	public:
		fs::path mFolder;
		bool mbRunning;

	private:
		HANDLE mhDir;

		// alertable thread, use run user-mode apc.
		std::thread mThread;
		// lock parse data
		std::mutex mDataMutex;

		// data need convert to PFILE_NOTIFY_INFORMATION
		/*alignas(alignof(DWORD))*/
		std::vector<std::byte> mWriteBuffer;
		/*alignas(alignof(DWORD))*/
		std::vector<std::byte> mReadBuffer;

		OVERLAPPED mOverlapped;
		LPOVERLAPPED_COMPLETION_ROUTINE mpCompletionRoutine;
	};
}

namespace freeze
{
	template<Stringify S>
	bool watchor::set_folder(S&& folder)
	{
		fs::path newFolder;
		if constexpr (std::is_same_v<S, fs::path>)
		{
			newFolder = detail::to_normal(folder);
		}
		else
		{
			newFolder = detail::to_normal(fs::path{ folder });
		}

		if (!(detail::check_exist(newFolder) && fs::is_directory(newFolder)))
		{
			mbRunning = false;
			OutputDebugString(L"Error, not a folder.\n");
			return false;
		}

		if (mhDir)
		{
			if (mbRunning && (newFolder == mFolder))
			{
				return true;
			}
			else
			{
				stop();

				CloseHandle(mhDir);
				mhDir = nullptr;
			}
		}

		mFolder = newFolder;
		mhDir = CreateFile(
			mFolder.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr
		);
		if (INVALID_HANDLE_VALUE == mhDir)
		{
			mbRunning = false;

			// error
			auto err = GetLastError();
			auto _msg = std::format(L"CreateFile Error: {}.\n"sv, err);
			OutputDebugString(_msg.c_str());

			return false;
		}

		mbRunning = true;
		return true;
	}
}

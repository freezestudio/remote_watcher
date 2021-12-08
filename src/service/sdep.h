//
// service dependence head files
//

#include <thread>
#include <vector>

#include "nats.h"

#define SERVICE_CONTROL_NETWORK_CONNECT    129
#define SERVICE_CONTROL_NETWORK_DISCONNECT 130


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

namespace freeze
{
	enum class accept_type
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

		bool start(accept_type acctype = accept_type::overlapped);
		void stop();

	public:
		void on_data(DWORD dwNumberOfBytesTransfered);
		void do_data(PFILE_NOTIFY_INFORMATION info);

	public:
		static void ApcCallback(ULONG_PTR parameter);

		static void OverlappedCompletionRoutine(
			DWORD dwErrorCode,
			DWORD dwNumberOfBytesTransfered,
			LPOVERLAPPED lpOverlapped);

		static void OverlappedCompletionResult(LPOVERLAPPED, LPVOID lpContext);
		static void OverlappedCompletionStatus(LPOVERLAPPED, LPVOID lpContext);

	private:
		void _move(watchor&&);
		void _reset_buffer(DWORD = 1024 * 1024);

	public:
		fs::path mFolder;
		bool mbRunning;

	private:
		HANDLE mhDir;

		// alertable thread, use run user-mode apc.
		std::thread mThread;

		// data need convert to PFILE_NOTIFY_INFORMATION
		alignas(alignof(DWORD))
		std::vector<std::byte> mWriteBuffer;
		alignas(alignof(DWORD))
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
		if constexpr (std::is_same_v<S, fs::path>)
		{
			mFolder = folder;
		}
		else
		{
			mFolder = fs::path{ folder };
		}

		if (!fs::exists(mFolder))
		{
			mbRunning = false;
			// error, not a folder
			return false;
		}

		if (mhDir)
		{
			stop();

			CloseHandle(mhDir);
			mhDir = nullptr;
		}

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

			auto err = GetLastError();
			// error

			return false;
		}

		mbRunning = true;
		return true;
	}
}

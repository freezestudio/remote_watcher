#include "dep.h"
#include "sdep.h"

namespace freeze::detail
{
	inline std::string to_str(freeze::accept_type t)
	{
		std::string s;
		switch (t)
		{
		case freeze::accept_type::result: s = "result"s; break;
		case freeze::accept_type::status: s = "status"s; break;
		case freeze::accept_type::overlapped: s = "overlapped"s; break;
		default:
			break;
		}
		return s;
	}

	inline std::string to_str(DWORD notify)
	{
		std::string s;
		switch (notify)
		{
		case FILE_ACTION_ADDED:s = "add"s; break;
		case FILE_ACTION_REMOVED:s = "remove"s; break;
		case FILE_ACTION_MODIFIED:s = "modify"s; break;
		case FILE_ACTION_RENAMED_OLD_NAME:s = "rename-old-name"s; break;
		case FILE_ACTION_RENAMED_NEW_NAME:s = "rename-new-name"s; break;
		default:
			break;
		}
		return s;
	}
}

namespace freeze
{
	watchor::watchor()
		: mbRunning{ false }
		, mhDir{ nullptr }
		, mOverlapped{}
		, mpCompletionRoutine{ nullptr }
        // TODO: modify this thread
	, mThread([]() { while (true) { SleepEx(INFINITE, TRUE); }})
	{
		_reset_buffer();

		// will block
		//if (mThread.joinable())
		//{
		//	mThread.join();
		//}

		// will loss data
		//mThread.detach();
	}

	watchor::~watchor()
	{
		stop();
	}

	watchor::watchor(watchor&& rhs) noexcept
	{
		_move(std::forward<watchor&&>(rhs));
	}

	watchor& watchor::operator=(watchor&& rhs) noexcept
	{
		_move(std::forward<watchor&&>(rhs));
		return *this;
	}

	void watchor::_move(watchor&& rhs)
	{
		mbRunning = std::move(std::exchange(rhs.mbRunning, false));
		mhDir = std::move(std::exchange(rhs.mhDir, nullptr));
		mOverlapped = std::move(std::exchange(rhs.mOverlapped, {}));
		mpCompletionRoutine = std::move(std::exchange(rhs.mpCompletionRoutine, nullptr));
		mThread = std::move(std::exchange(rhs.mThread, {}));

		// move buffers
		mReadBuffer = std::move(std::exchange(rhs.mReadBuffer, {}));
		mWriteBuffer = std::move(std::exchange(rhs.mWriteBuffer, {}));
	}

	void watchor::_reset_buffer(DWORD size)
	{
		mReadBuffer.resize(size);
		mWriteBuffer.resize(size);
	}
}

namespace freeze
{
	// expect to run in other alertable threads.
	bool watchor::start(accept_type acctype)
	{
		//std::cout << "watchor::start -> " << std::boolalpha << mbRunning << std::endl;

		if (!mbRunning)
		{
			return false;
		}

		if (!mhDir)
		{
			mbRunning = false;
			return false;
		}

		mOverlapped = {};
		switch (acctype)
		{
		default:
			break;
		case freeze::accept_type::result:
			// use GetOverlappedResult
			mpCompletionRoutine = nullptr;
			mOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			break;
			//[[fallthrough]];
		case freeze::accept_type::status:
			// use GetQueuedCompletionStatus
			// use CreateIoCompletionPort associate mhDir
			mpCompletionRoutine = nullptr;
			break;
		case freeze::accept_type::overlapped:
			mOverlapped.hEvent = this;
			mpCompletionRoutine = watchor::OverlappedCompletionRoutine;
			break;
		}

		if (mWriteBuffer.empty())
		{
			_reset_buffer();
		}

		BOOL ret = FALSE;
		do
		{
			ret = ReadDirectoryChangesW(
				mhDir,
				reinterpret_cast<LPVOID>(mWriteBuffer.data()),
				static_cast<DWORD>(mWriteBuffer.size()),
				TRUE, // monitors subtree
				gNotifyFilter,
				nullptr, // bytes returned, for asynchronous call, undefined.
				&mOverlapped, // asynchronous operation
				mpCompletionRoutine // maybe nullptr, run in alertable thread.
			);
			if (!ret)
			{
				auto err = GetLastError();
				if (err == ERROR_INVALID_FUNCTION)
				{
					// the network redirector or the target file system does not support this operation
					mbRunning = false;
					return false;
				}
				else if (err == ERROR_INVALID_PARAMETER)
				{
					// buffer size > 64 kb and monitoring a directory over the network.
					// TODO: shrink buffer size
					_reset_buffer(64 * 1024);
				}
				else if (err == ERROR_NOACCESS)
				{
					// buffer is not aligned on a DWORD boundary
					// TODO: set alignas(DWORD)
					//std::align(alignof(DWORD), sizeof(DWORD), (void*)mWriteBuffer.data(), mWriteBuffer.size());
				}
				else
				{
					mbRunning = false;
					return false;
				}
			}
		} while (!ret);

		// out, run in alertable threads.
		//QueueUserAPC(
		//	watchor::ApcCallback,
		//	mThread.native_handle(),
		//	reinterpret_cast<ULONG_PTR>(this));

		if (accept_type::result == acctype)
		{
			watchor::OverlappedCompletionResult(&mOverlapped, this);
		}

		if (accept_type::status == acctype)
		{
			watchor::OverlappedCompletionStatus(&mOverlapped, this);
		}

		//std::cout << "watchor::start, accept_type=" << detail::to_str(acctype) << std::endl;
		return true;
	}

	void watchor::stop()
	{
		mbRunning = false;
		if (mhDir)
		{
			CancelIo(mhDir);
			CloseHandle(mhDir);
			mhDir = nullptr;
		}
		mOverlapped = {};
		mpCompletionRoutine = nullptr;

		// TODO: how to exit thread?
		std::exchange(mThread, {});
	}
}

namespace freeze::detail
{
	struct notify_information
	{
		LARGE_INTEGER size;
		LARGE_INTEGER creation;
		LARGE_INTEGER modification;
		LARGE_INTEGER change;
		DWORD attributes;
		bool folder; // maybe mistake
		DWORD action;
		std::string filename;
	};

	std::string to_utf8(const wchar_t* input, int length)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0, input, length, NULL, 0, NULL, NULL);
		char* output = new char[len + 1];
		WideCharToMultiByte(CP_UTF8, 0, input, length, output, len, NULL, NULL);
		output[len] = '\0';
		std::string str(output);
		delete[] output;
		return str;
	}

	std::wstring to_utf16(std::string const& input) 
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
		WCHAR* output = new WCHAR[len];
		MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output, len);
		std::wstring res(output);
		delete[] output;
		return res;
	}

	notify_information folder_info(fs::path const& folder, DWORD action, int len, wchar_t const* name)
	{
		notify_information info;
		info.action = action;
		info.filename = to_utf8(name, len);

		auto folder_path = folder / name;
		if (!fs::exists(folder_path))
		{
			return info;
		}

		info.folder = fs::is_directory(folder_path);
		if (fs::is_regular_file(folder_path))
		{
			info.size.QuadPart = fs::file_size(folder_path);
		}
		else
		{
			info.size.QuadPart = 0;
		}

		WIN32_FILE_ATTRIBUTE_DATA data;
		auto ret = GetFileAttributesEx(folder_path.c_str(), GetFileExInfoStandard, (LPVOID)(&data));
		if (ret)
		{
			info.attributes = data.dwFileAttributes;
			info.creation = LARGE_INTEGER{
				data.ftCreationTime.dwLowDateTime,
				static_cast<LONG>(data.ftCreationTime.dwHighDateTime),
			};
			info.modification = LARGE_INTEGER{
				data.ftLastWriteTime.dwLowDateTime,
				static_cast<LONG>(data.ftLastWriteTime.dwHighDateTime),
			};
			info.change = LARGE_INTEGER{
			data.ftLastAccessTime.dwLowDateTime,
			static_cast<LONG>(data.ftLastAccessTime.dwHighDateTime),
			};
		}
		else
		{
			info.attributes = static_cast<DWORD>(fs::status(folder_path).type());
			//info.modification = fs::last_write_time(folder).time_since_epoch().count();
		}
		return info;
	}

	// test only! global data used in golbal thread.
	std::vector<notify_information> _local_notify_info{};
}

namespace freeze
{
	void watchor::on_data(DWORD dwNumberOfBytesTransfered) try
	{
		// std::cout << "watchor::on_data, bytes=" << dwNumberOfBytesTransfered << std::endl;
		if (dwNumberOfBytesTransfered == 0 || dwNumberOfBytesTransfered > mReadBuffer.size())
		{
			// need more buffer size
			return;
		}

		auto pInfo = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(mReadBuffer.data());
		if (!pInfo)
		{
			// error
			// std::cout << "watchor::on_data, notify info is null." << std::endl;
			return;
		}

		detail::_local_notify_info.clear();
		while (true)
		{
			do_data(pInfo);

			if (pInfo->NextEntryOffset == 0)
			{
				break;
			}
			pInfo = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(mReadBuffer.data() + pInfo->NextEntryOffset);
		}
		mReadBuffer.clear();

		auto ret = QueueUserAPC(watchor::ApcCallback, mThread.native_handle(), (ULONG_PTR)(&detail::_local_notify_info));
		if (!ret)
		{
			// When the thread is in the process of being terminated,
			// calling QueueUserAPC to add to the thread's APC queue will fail with ERROR_GEN_FAILURE (31).
			auto err = GetLastError();
			// std::cout << "watchor::on_data Error: " << err << std::endl;
		}
	}
	catch (std::runtime_error& e)
	{
		// std::cout << "error: " << e.what() << std::endl;
	}

	void watchor::do_data(PFILE_NOTIFY_INFORMATION info)
	{
		if (info->Action)
		{
			auto name = std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t)) + L"\0";
			detail::_local_notify_info.emplace_back(
				detail::folder_info(
					mFolder, 
					info->Action, 
					info->FileNameLength / sizeof(wchar_t), 
					info->FileName)
			);
		}
	}
}

namespace freeze
{
	/*static*/
	void watchor::ApcCallback(ULONG_PTR parameter)
	{
		auto pData = reinterpret_cast<std::vector<detail::notify_information>*>(parameter);
		if (!pData)
		{
			// std::cout << "monitor: error, data is null." << std::endl;
			// error
			return;
		}

        // TODO: to_utf16(filename)

        // test only
		for (auto d : *pData)
		{
			// output data
			// std::cout << "monitor: name=" << d.filename 
			// 	<< ", action=" << detail::to_str(d.action)
			// 	<< ", is-folder=" << std::boolalpha << d.folder
			// 	<< std::endl;
		}

		// notify monitor event
	}

	//struct OVERLAPPED
	//{
	//    ULONG_PTR Internal;
	//    ULONG_PTR InternalHigh;
	// 
	//    union
	//    {
	//        struct
	//        {
	//            DWORD Offset;
	//            DWORD OffsetHigh;
	//        } DUMMYSTRUCTNAME;
	//        PVOID Pointer;
	//    } DUMMYUNIONNAME;
	// 
	//    HANDLE    hEvent; // nullptr, or CreateEvent(manual-reset)
	//};
	// GetOverlappedResult
	// HasOverlappedIoCompleted
	// CancelIo

	/*
	* maybe run in alertable thread.
	*
	*/
	/* static */
	void watchor::OverlappedCompletionRoutine(
		DWORD dwErrorCode,
		DWORD dwNumberOfBytesTransfered,
		LPOVERLAPPED lpOverlapped)
	{
		auto pWatchor = reinterpret_cast<watchor*>(lpOverlapped->hEvent);
		if (!pWatchor)
		{
			// error
			// std::cout << "Overlapped Completion Routine: watchor pointer is null." << std::endl;
			return;
		}

		if (dwErrorCode != ERROR_SUCCESS)
		{
			// error
			auto err = GetLastError();

			if (err == ERROR_OPERATION_ABORTED)
			{
				// operation abort
				// std::cout << "Overlapped Completion Routine: Error{ERROR_OPERATION_ABORTED}." << std::endl;
				return;
			}
			else if (err == ERROR_INVALID_PARAMETER)
			{
				// resize buffer
				// std::cout << "Overlapped Completion Routine: Error{ERROR_INVALID_PARAMETER}." << std::endl;
			}
			else
			{
				// std::cout << "Overlapped Completion Routine: Error{Unknown}." << std::endl;
			}
			// std::cout << "Overlapped Completion Routine Error: " << err << std::endl;
			return;
		}

		// ERROR_SUCCESS, but maybe has warnning
		// DWORD num_trans;
		//GetOverlappedResult(lpOverlapped->hEvent->mhDir, lpOverlapped, &num_trans, TRUE);
		// auto err = GetLastError();

		std::swap(pWatchor->mWriteBuffer, pWatchor->mReadBuffer);
		pWatchor->on_data(dwNumberOfBytesTransfered);
	}

	/* static */
	void watchor::OverlappedCompletionResult(LPOVERLAPPED lpOverlapped, LPVOID lpContext)
	{
		auto pWatchor = reinterpret_cast<watchor*>(lpContext);

		DWORD bytes_transfer;
		auto ret = GetOverlappedResult(pWatchor->mhDir, lpOverlapped, &bytes_transfer, TRUE);
		if (!ret)
		{
			// error
			return;
		}

		std::swap(pWatchor->mWriteBuffer, pWatchor->mReadBuffer);
		pWatchor->on_data(bytes_transfer);

		// manual-reset event
		ResetEvent(lpOverlapped->hEvent);
	}

	/* static */
	void watchor::OverlappedCompletionStatus(LPOVERLAPPED lpOverlapped, LPVOID lpContext)
	{
		auto pWatchor = reinterpret_cast<watchor*>(lpContext);
		auto hPort = CreateIoCompletionPort(pWatchor->mhDir, nullptr, 0, 0);
		if (!hPort)
		{
			// error
			return;
		}

		DWORD bytes_transfer;
		ULONG_PTR key;
		auto ret = GetQueuedCompletionStatus(hPort, &bytes_transfer, &key, &lpOverlapped, INFINITE);
		if (!ret)
		{
			// error
			return;
		}

		// process data ...
		std::swap(pWatchor->mWriteBuffer, pWatchor->mReadBuffer);
		pWatchor->on_data(bytes_transfer);
	}
}

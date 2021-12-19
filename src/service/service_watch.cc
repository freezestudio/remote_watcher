#include "service_watch.h"

namespace freeze::detail
{
	static std::wstring to_str(freeze::response_type t)
	{
		std::wstring s;
		switch (t)
		{
		case freeze::response_type::result: s = L"result"s; break;
		case freeze::response_type::status: s = L"status"s; break;
		case freeze::response_type::overlapped: s = L"overlapped"s; break;
		default:
			break;
		}
		return s;
	}

	static std::wstring to_str(DWORD notify)
	{
		std::wstring s;
		switch (notify)
		{
			//case FILE_ACTION_ADDED:s = L"add"s; break;
			//case FILE_ACTION_REMOVED:s = L"remove"s; break;
			//case FILE_ACTION_MODIFIED:s = L"modify"s; break;
			//case FILE_ACTION_RENAMED_OLD_NAME:s = L"rename-old-name"s; break;
			//case FILE_ACTION_RENAMED_NEW_NAME:s = L"rename-new-name"s; break;
		case FILE_ACTION_ADDED: [[fallthrough]];
		case FILE_ACTION_RENAMED_NEW_NAME:s = L"create"s; break;
		case FILE_ACTION_REMOVED: [[fallthrough]];
		case FILE_ACTION_RENAMED_OLD_NAME:s = L"remove"s; break;
		case FILE_ACTION_MODIFIED:s = L"modify"s; break;
		default:
			break;
		}
		return s;
	}
}

namespace freeze
{
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

	decltype(auto) watchor::_start_thread()
	{
		auto _thread = std::thread([this]()
			{
				while (true)
				{
					// Alertable for apc.
					SleepEx(INFINITE, TRUE);
					if (!mbRunning)
					{
						break;
					}
				}
			});
		return _thread;
	}

	void watchor::_stop_thread()
	{
		if (!mbRunning && mThread.joinable())
		{
			QueueUserAPC([](auto) {}, mThread.native_handle(), 0);
			mThread.join();
		}
	}
}

namespace freeze
{
	watchor::watchor()
		: mbRunning{ false }
		, mhDir{ nullptr }
		, mOverlapped{}
		, mpCompletionRoutine{ nullptr }
	{
		mThread = _start_thread();
		reset_buffer();
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


	void watchor::reset_buffer(DWORD size)
	{
		mReadBuffer.resize(size);
		mWriteBuffer.resize(size);
	}
}

namespace freeze
{
	// expect to run in other alertable threads.
	bool watchor::start(response_type restype)
	{
		if (!mbRunning)
		{
			_stop_thread();
			return false;
		}

		if (!mhDir)
		{
			mbRunning = false;
			_stop_thread();
			return false;
		}

		mOverlapped = {};
		switch (restype)
		{
		default:
			break;
		case freeze::response_type::result:
			mpCompletionRoutine = nullptr;
			mOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			break;
		case freeze::response_type::status:
			mpCompletionRoutine = nullptr;
			break;
		case freeze::response_type::overlapped:
			mOverlapped.hEvent = this;
			mpCompletionRoutine = watchor::OverlappedCompletionRoutine;
			break;
		}

		if (mWriteBuffer.empty())
		{
			reset_buffer();
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
				nullptr, // bytes returned. for asynchronous call, undefined.
				&mOverlapped, // asynchronous operation
				mpCompletionRoutine // maybe nullptr, run in alertable thread.
			);
			if (!ret)
			{
				stop();
			}
		} while (!ret);

		// out, run in alertable threads.
		if (response_type::result == restype)
		{
			QueueUserAPC([](auto param)
				{
					auto self = reinterpret_cast<watchor*>(param);
					watchor::OverlappedCompletionResult(&self->mOverlapped, self);
				}, mThread.native_handle(), (ULONG_PTR)(this));
		}
		else if (response_type::status == restype)
		{
			QueueUserAPC([](auto param)
				{
					auto self = reinterpret_cast<watchor*>(param);
					watchor::OverlappedCompletionStatus(&self->mOverlapped, self);
				}, mThread.native_handle(), (ULONG_PTR)(this));
		}

		return true;
	}

	void watchor::stop()
	{
		mbRunning = false;
		_stop_thread();

		if (mhDir)
		{
			CancelIo(mhDir);
			CloseHandle(mhDir);
			mhDir = nullptr;
		}
		mOverlapped = {};
		mpCompletionRoutine = nullptr;
	}
}

namespace freeze::detail
{
	notify_information_w folder_info_w(fs::path const& folder, DWORD action, std::wstring const& name)
	{
		notify_information_w info{};
		info.action = action;
		info.filename = name;

		auto folder_path = (folder / name).lexically_normal();
		if (!fs::exists(folder_path))
		{
			return info;
		}

		info.isfile = !fs::is_directory(folder_path);
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
			info.isfile = !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		}
		else
		{
			info.attributes = static_cast<DWORD>(fs::status(folder_path).type());
			//info.modification = fs::last_write_time(folder).time_since_epoch().count();
		}
		return info;
	}

	// test only! global data should used in golbal thread.
	std::vector<notify_information_w> g_local_notify_info_w{};
	std::vector<notify_information_w>& get_changed_information()
	{
		return g_local_notify_info_w;
	}
}

namespace freeze
{
	void watchor::notify_information(DWORD dwNumberOfBytesTransfered)
	{
		if (dwNumberOfBytesTransfered == 0 || dwNumberOfBytesTransfered > mReadBuffer.size())
		{
			DEBUG_STRING(L"watchor::on_data, maybe need more buffer.\n");
			return;
		}

		std::lock_guard<std::mutex> lock(mDataMutex);
		std::swap(mWriteBuffer, mReadBuffer);

		// TODO: early return to start()
		// TODO: move to other thread run.

		auto info_ptr = mReadBuffer.data();
		if (!info_ptr)
		{
			// error
			DEBUG_STRING(L"watchor::on_data, notify info is null.\n");
			return;
		}

		detail::g_local_notify_info_w.clear();
		while (true)
		{
			auto pInfo = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(info_ptr);
			parse_information(pInfo);
			if (pInfo->NextEntryOffset == 0 || pInfo->NextEntryOffset > dwNumberOfBytesTransfered)
			{
				break;
			}
			info_ptr += pInfo->NextEntryOffset;
		}

		// Test Only!
		auto _idx = 1;
		for (auto x : detail::g_local_notify_info_w)
		{
			DEBUG_STRING(L"{}: watch: action={}, name={}, isfile={}\n"sv, _idx++, x.action, x.filename, x.isfile);
		}

		auto ret = QueueUserAPC(
			watchor::OverlappedCompletionCallback,
			mThread.native_handle(),
			(ULONG_PTR)(&detail::g_local_notify_info_w));
		if (!ret)
		{
			auto err = GetLastError();
			// When the thread is in the process of being terminated,
			// calling QueueUserAPC to add to the thread's APC queue
			// will fail with ERROR_GEN_FAILURE (31).
			if (err == ERROR_GEN_FAILURE)
			{
				// re-create thread
				mThread = _start_thread();
				start();
				return;
			}

			DEBUG_STRING(L"watchor::on_data Error: {}.\n"sv, err);

			stop();
		}
	}

	void watchor::parse_information(PFILE_NOTIFY_INFORMATION info)
	{
		if (info->Action < 1 || info->Action > 5)
		{
			DEBUG_STRING(L"when parse notify information, recv an bad action: {}\n"sv, info->Action);
			return;
		}

		auto name = std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t));
		auto notify_info = detail::folder_info_w(mFolder, info->Action, name);
		detail::g_local_notify_info_w.emplace_back(notify_info);
	}
}

namespace freeze
{
	/*static*/
	void watchor::OverlappedCompletionCallback(ULONG_PTR Parameter)
	{
		auto pData = reinterpret_cast<std::vector<detail::notify_information_w>*>(Parameter);
		if (!pData)
		{
			DEBUG_STRING(L"ApcCallback: notify-information is null.\n");
			// error
			return;
		}

		if (pData->size() > 0)
		{
			global_reason_signal.notify_reason(sync_reason_send_payload);
		}
	}
}

namespace freeze
{
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
			DEBUG_STRING(L"Overlapped Completion Routine: Watchor Pointer is nullptr.\n");
			return;
		}

		if (dwErrorCode != ERROR_SUCCESS)
		{
			// error
			auto err = GetLastError();
			if (err == ERROR_INVALID_FUNCTION)
			{
				// the network redirector or the target file system does not support this operation
				pWatchor->stop();
			}
			if (err == ERROR_OPERATION_ABORTED)
			{
				// operation abort
				DEBUG_STRING(L"Overlapped Completion Routine: Error{ERROR_OPERATION_ABORTED}.\n");
				return;
			}
			else if (err == ERROR_INVALID_PARAMETER)
			{
				// buffer size > 64 kb and monitoring a directory over the network.
				// TODO: shrink buffer size
				pWatchor->reset_buffer(64 * 1024);
				// resize buffer
				DEBUG_STRING(L"Overlapped Completion Routine: Error{ERROR_INVALID_PARAMETER}.\n");
			}
			else if (err == ERROR_NOACCESS)
			{
				// buffer is not aligned on a DWORD boundary
				// TODO: set alignof(n) to alignas(DWORD)
			}
			else
			{
				DEBUG_STRING(L"Overlapped Completion Routine: Error{Unknown}.\n");
			}
			DEBUG_STRING(L"Overlapped Completion Routine Error: {}.\n"sv, err);
			return;
		}

		pWatchor->notify_information(dwNumberOfBytesTransfered);
	}

	/* static */
	void watchor::OverlappedCompletionResult(LPOVERLAPPED lpOverlapped, LPVOID lpContext)
	{
		auto pWatchor = reinterpret_cast<watchor*>(lpContext);
		if (!pWatchor)
		{
			//error
			return;
		}

		DWORD bytes_transfer;
		auto ret = GetOverlappedResult(pWatchor->mhDir, lpOverlapped, &bytes_transfer, TRUE);
		if (!ret)
		{
			// error
			return;
		}

		pWatchor->notify_information(bytes_transfer);

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

		pWatchor->notify_information(bytes_transfer);
	}
}

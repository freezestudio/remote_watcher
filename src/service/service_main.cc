//
// exe service entry
//

#include "service.h"
#include "service_extern.h"

//
// TODO: #include <netlistmgr.h>
// com: INetworkConnection::get_IsConnected
//

/* extern */
std::wstring g_wcs_ip{};

int64_t reset_ip_address(std::wstring const& wcs_ip)
{
	if (!g_wcs_ip.empty())
	{
		auto _ip = freeze::detail::make_ip_address(g_wcs_ip);
		if (freeze::detail::check_ip_address(_ip))
		{
			return static_cast<int64_t>(_ip); // old ip success
		}
		else
		{
			return -1; // bad ip error!
		}
	}

	uint32_t ip = 0;
	std::wstring str_ip;
	bool need_save = false;
	if (wcs_ip.empty())
	{
		ip = freeze::detail::read_ip();
	}
	else
	{
		str_ip = wcs_ip;
		need_save = true;
	}

	if (!str_ip.empty())
	{
		ip = freeze::detail::make_ip_address(str_ip);
	}

	if (!freeze::detail::check_ip_address(ip))
	{
		// 4,294,967,295
		//constexpr auto _max = (std::numeric_limits<uint32_t>::max)();
		//constexpr auto _max = static_cast<uint32_t>(-1);
		return 0; // str->num convert error!
	}

	if (need_save)
	{
		if (!freeze::detail::save_ip(ip))
		{
			return -2; // save ip error!
		}
	}

	auto mbs_ip = freeze::detail::parse_ip_address(ip);
	g_wcs_ip = freeze::detail::to_utf16(mbs_ip);

	if (!g_wcs_ip.empty())
	{
		return static_cast<int64_t>(ip); // new ip success
	}
	else
	{
		return -3; // mbs->wcs convert error!
	}
}

std::wstring reset_ip_error(int64_t code)
{
	if (code > 0)
	{
		return L"error success";
	}
	else if (code == 0)
	{
		return L"error convert str to num ip";
	}
	else if (code == -1)
	{
		return L"error bad ip";
	}
	else if (code == -2)
	{
		return L"error save ip";
	}
	else if (code == -3)
	{
		return L"error convert mbs to wcs ip";
	}
	else
	{
		return L"error unknown";
	}
}

/**
 * @brief 服务入口
 * @param argc 参数个数
 * @param argv 参数列表
 */
void __stdcall ServiceMain(DWORD argc, LPWSTR* argv)
{
	// 初始化所有全局变量
	// 如果初始化时间不超过 1s, 可以直接设置服务状态为 SERVICE_RUNNING

	DEBUG_STRING(L"@rg ServiceMain: Service Starting ...\n");
	if (init_service())
	{
		int64_t remote_ip = 0;
		if (argc > 1)
		{
			for (auto i = 0; i < argc; ++i)
			{
				DEBUG_STRING(L"@rg ServiceMain: argc[{}], {}\n"sv, i, argv[i]);
			}
			remote_ip = reset_ip_address(argv[1]);
			DEBUG_STRING(L"@rg ServiceMain: argc={}, RemoteIP is: {}\n"sv, argc, g_wcs_ip);
		}
		else
		{
			remote_ip = reset_ip_address();
		}

		if (remote_ip < 0 || !freeze::detail::check_ip_address(static_cast<uint32_t>(remote_ip)))
		{
			DEBUG_STRING(L"@rg ServiceMain: remote ip not set, {}!\n"sv, reset_ip_error(remote_ip));
			stop_service();
			goto theend;
		}

		reset_work_folder();

		if (init_threadpool())
		{
			// block waitable-event
			loop_service();


			DEBUG_STRING(L"@rg ServiceMain: Sevice will stopping ...\n");
			stop_threadpool();
			stop_service();
		}
		else
		{
			DEBUG_STRING(L"@rg ServiceMain: Init Threadpool Failure.\n");
		}
	}
	else
	{
		DEBUG_STRING(L"@rg ServiceMain: Init Service Failure.\n");
	}


theend:
	DEBUG_STRING(L"@rg ServiceMain: Service Stopped.\n");
}

int __stdcall wmain()
{
#ifdef SERVICE_TEST
	ServiceMain(0, nullptr);
#else
	wchar_t _service_name[] = SERVICE_NAME;
	SERVICE_TABLE_ENTRY _dispatch_table[] = {
		{_service_name, static_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
		{nullptr, nullptr},
	};
	if (StartServiceCtrlDispatcher(_dispatch_table))
	{
		DEBUG_STRING(L"@rg main: Dispatch Service Successfully.\n");
	}
	else
	{
		DEBUG_STRING(L"@rg main: Dispatch Service Failure.\n");
	}
#endif
	DEBUG_STRING(L"@rg main: Done.\n");
}

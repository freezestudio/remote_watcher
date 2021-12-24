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

/**
 * @brief 服务入口
 * @param argc 参数个数
 * @param argv 参数列表
 */
void __stdcall ServiceMain(DWORD argc, LPWSTR* argv)
{
	// 初始化所有全局变量
	// 如果初始化时间不超过 1s, 可以直接设置服务状态为 SERVICE_RUNNING

	if (argc > 1)
	{
		g_wcs_ip = argv[1];
		DEBUG_STRING(L"@rg ServiceMain: argc={}, RemoteIP is: {}\n"sv, argc, g_wcs_ip);
	}
	for (auto i = 0; i < argc; ++i)
	{
		DEBUG_STRING(L"@rg ServiceMain: argc[{}], {}\n"sv, i, argv[i]);
	}

	if (!g_wcs_ip.empty())
	{
		DWORD ip = freeze::detail::make_ip_address(g_wcs_ip);
		if (ip == 0)
		{
			DEBUG_STRING(L"@rg ServiceMain: ip is wrong.\n");
			goto theend;
		}

		if (!freeze::detail::save_ip(ip))
		{
			DEBUG_STRING(L"@rg ServiceMain: save ip failure.\n");
			goto theend;
		}
	}
	else
	{
		DWORD ip = freeze::detail::read_ip();
		if (ip == 0)
		{
			DEBUG_STRING(L"@rg ServiceMain: read ip is wrong.\n");
			goto theend;
		}
		auto mcs_ip = freeze::detail::parse_ip_address(ip);
		g_wcs_ip = freeze::detail::to_utf16(mcs_ip);
	}

	if (g_wcs_ip.empty())
	{
		DEBUG_STRING(L"@rg ServiceMain: ip is null.\n");
		// stopped with error.
		goto theend;
	}

	if (freeze::detail::make_ip_address(g_wcs_ip) == 0)
	{
		DEBUG_STRING(L"@rg ServiceMain: ip is wrong.\n");
		// stopped with error.
		goto theend;
	}

	DEBUG_STRING(L"@rg ServiceMain: Service Starting ...\n");
	if (init_service())
	{
		reset_work_folder();
		if (init_threadpool())
		{
			run_service();
		}
	}

	stop_threadpool();
	stop_service();

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

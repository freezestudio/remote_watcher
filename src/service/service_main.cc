//
// exe service entry
//

#include "service.h"

//
// TODO: #include <netlistmgr.h>
// com: INetworkConnection::get_IsConnected
//

std::wstring wcs_ip{};

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
		wcs_ip = argv[1];
		auto _msg = std::format(L"@rg ServiceMain: argc={}, RemoteIP is: {}\n"sv, argc, wcs_ip);
		OutputDebugString(_msg.data());
	}
	for (auto i = 0; i < argc; ++i)
	{
		auto _msg = std::format(L"@rg ServiceMain: argc[{}], {}\n"sv, i, argv[i]);
		OutputDebugString(_msg.c_str());
	}

	// TODO: read (ip,token) from .ini
	// ...

	if (wcs_ip.empty())
	{
		OutputDebugString(L"@rg ServiceMain: ip is null.\n");
		// stopped with error.
		goto theend;
	}
	
		if (freeze::detail::make_ip_address(wcs_ip) == 0)
		{
			OutputDebugString(L"@rg ServiceMain: ip is wrong.\n");
			// stopped with error.
			goto theend;
		}
		
			OutputDebugString(L"@rg ServiceMain: rgmsvc starting ...\n");
			if(init_service())
			{
				if(init_threadpool())
				{
					run_service();
				}
			}

	stop_threadpool();
	stop_service();
	
theend:
	OutputDebugString(L"@rg ServiceMain: rgmsvc stopped.\n");
}

int __stdcall wmain()
{
#ifdef SERVICE_TEST
	g_work_folder = fs::path{ L"f:/templ/abc"s };

	wchar_t arg1[] = L"rgmsvc";
	wchar_t arg2[] = L"192.168.2.95";
	wchar_t* argv[] = {
		arg1,
		arg2,
	};
	ServiceMain(2, argv);
#else
	wchar_t _service_name[] = SERVICE_NAME;
	SERVICE_TABLE_ENTRY _dispatch_table[] = {
		{_service_name, static_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
		{nullptr, nullptr},
	};
	if (StartServiceCtrlDispatcher(_dispatch_table))
	{
		OutputDebugString(L"@rg main: Dispatch Service Successfully.\n");
	}
	else
	{
		OutputDebugString(L"@rg main: Dispatch Service Failure.\n");
	}
#endif
	OutputDebugString(L"@rg main: Done.\n");
}

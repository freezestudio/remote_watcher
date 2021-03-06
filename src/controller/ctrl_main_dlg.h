#ifndef CTRL_MAIN_DLG_H
#define CTRL_MAIN_DLG_H

#define IP_ADDRESS_LEN 16
using namespace std::literals;

class MainDlg
	: public CDialogImpl<MainDlg>,
	public CUpdateUI<MainDlg>,
	public CMessageFilter,
	public CIdleHandler
{
public:
	// use in: CDialogImpl::Create(...)
	static constexpr auto IDD = IDD_MAINDLG;

	// Controls
public:
	CImageList mInstallButtonImage;
	CImageList mUninstallButtonImage;
	CBitmapButton mInstallButton;
	CBitmapButton mUninstallButton;
	CIPAddressCtrl mIP;

	BOOL mEnableInstall = FALSE;
	BOOL mEnableUninstall = FALSE;
	bool mAutoStart = true;

public:
	virtual BOOL PreTranslateMessage(LPMSG pMsg) /* override */
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	virtual BOOL OnIdle() /* override */
	{
		UIUpdateChildWindows();
		return FALSE;
	}

public:
	BEGIN_UPDATE_UI_MAP(MainDlg)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(MainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		//SYS_CLOSE_BUTTON
		COMMAND_ID_HANDLER_EX(2, OnClose)
		// PushButton
		COMMAND_ID_HANDLER_EX(IDC_BTN_INSTALL, OnInstall)
		COMMAND_ID_HANDLER_EX(IDC_BTN_UNINSTALL, OnUninstall)
		// Checkbox
		COMMAND_ID_HANDLER_EX(IDC_CHECK_AUTO_START, OnCheckAutoStart)
		// SysLink of Notify
		//NOTIFY_ID_HANDLER_EX(IDC_SYSLINK_START, OnStartService)
		//COMMAND_ID_HANDLER_EX(IDC_SYSLINK_STOP, OnStopService)
		MSG_WM_NOTIFY(OnNotify)
		END_MSG_MAP()

public:
	BOOL OnInitDialog(CWindow, LPARAM)
	{
		CenterWindow();

		DWORD state = 0;
		auto is_installed = is_service_installed(&state);
		mEnableInstall = is_installed ? FALSE : TRUE;
		mEnableUninstall = is_installed ? TRUE : FALSE;
		if (!is_installed)
		{
			SetDlgItemText(IDC_STATUS, L"未安装!");
		}
		else
		{
			if (state == SERVICE_RUNNING)
			{
				SetDlgItemText(IDC_STATUS, L"已安装, 服务已启动!");
			}
			else if (state == SERVICE_STOPPED)
			{
				SetDlgItemText(IDC_STATUS, L"已安装, 服务已停止!");
			}
			else
			{
				SetDlgItemText(IDC_STATUS, L"已安装!");
			}
		}

		CheckDlgButton(IDC_CHECK_AUTO_START, BST_CHECKED);
		_CheckLinkEnable(is_installed);

		_SetIcon();
		_SetIcon(true);
		_Subclass();
		_AddToMessageLoop();

		UIAddChildWindowContainer(m_hWnd);

		return TRUE;
	}

	void OnDestroy()
	{
		_UnSubclass();
		_RemoveFromMessageLoop();
	}

	void OnClose(UINT uNotifyCode, int nID, CWindow /* wndCtl */)
	{
		_Close();
	}

	void OnInstall(UINT uNotifyCode, int nID, CWindow /* wndCtl */)
	{
		wchar_t ip[IP_ADDRESS_LEN]{};
		DWORD addr = _IpAddress(ip, IP_ADDRESS_LEN);
		if (addr == 0)
		{
			auto ret = MessageBox(L"未填写 IP 地址,\n是否自动使用本机 IP 地址?", L"监控服务", MB_ICONWARNING | MB_YESNO);
			if (ret == IDNO)
			{
				return;
			}
		}

		if (install_service(mAutoStart))
		{
			save_ip(addr);
			if (mAutoStart)
			{
				Sleep(1000);
				auto started = start_service(ip);

				DEBUG_STRING(L"Install Start Service Result: {}\n"sv, started);
				if (started)
				{
					mEnableInstall = FALSE;
					mEnableUninstall = TRUE;
					SetDlgItemText(IDC_STATUS, L"已安装, 服务已启动!");
				}
				else
				{
					mEnableInstall = TRUE;
					mEnableUninstall = FALSE;
					SetDlgItemText(IDC_STATUS, L"已安装, 服务启动失败!");
				}
			}
			else
			{
				mEnableInstall = FALSE;
				mEnableUninstall = TRUE;
				SetDlgItemText(IDC_STATUS, L"已安装, 服务已停止!");
			}
			_SetButtonEnabled();
			_CheckLinkEnable(true);
		}
		else
		{
			DEBUG_STRING(L"Install Service Failure.\n");
			SetDlgItemText(IDC_STATUS, L"安装失败!");
			mEnableInstall = TRUE;
			mEnableUninstall = FALSE;
			_SetButtonEnabled();
		}
	}

	void OnUninstall(UINT uNotifyCode, int nID, CWindow /* wndCtl */)
	{
		if (stop_service())
		{
			SetDlgItemText(IDC_STATUS, L"服务已停止!");
			auto uninstalled = uninstall_service();
			DEBUG_STRING(L"Uninstall Service Result: {}\n"sv, uninstalled);
			if (uninstalled)
			{
				mEnableInstall = TRUE;
				mEnableUninstall = FALSE;
				SetDlgItemText(IDC_STATUS, L"已卸载!");
				// TODO: remove HKEY_CURRENT_USER\Software\richgolden\rgmsvc
				// TODO: remove HKEY_USERS\.DEFAULT\Software\richgolden\rgmsvc
			}
			else
			{
				mEnableInstall = FALSE;
				mEnableUninstall = TRUE;
				SetDlgItemText(IDC_STATUS, L"服务已停止, 卸载失败!");
			}
			_SetButtonEnabled();
			_CheckLinkEnable(false);
		}
		else
		{
			SetDlgItemText(IDC_STATUS, L"停止服务失败!");
			OutputDebugString(L"Stop Service Failure.\n");
			mEnableInstall = TRUE;
			mEnableUninstall = FALSE;
			_SetButtonEnabled();
		}
	}

	void OnCheckAutoStart(UINT uNotifyCode, int nID, CWindow /* wndCtl */)
	{
		mAutoStart = IsDlgButtonChecked(nID) == BST_CHECKED;
	}

	LRESULT OnNotify(int idCtrl, LPNMHDR pnmh)
	{
		if (pnmh->code != NM_CLICK)
		{
			return FALSE;
		}

		if (idCtrl == IDC_SYSLINK_START)
		{
			return OnStartService(pnmh);
		}

		if (idCtrl == IDC_SYSLINK_STOP)
		{
			return OnStopService(pnmh);
		}

		if (idCtrl == IDC_SYSLINK_RESTART)
		{
			OnStopService(pnmh);
			OnStartService(pnmh);
			return TRUE;
		}

		return FALSE;
	}

	LRESULT OnStartService(LPNMHDR pnmh)
	{
		wchar_t ip[IP_ADDRESS_LEN]{};
		DWORD addr = _IpAddress(ip, IP_ADDRESS_LEN);
		if (addr == 0)
		{
			auto ret = MessageBox(L"未填写 IP 地址,\n是否自动使用本机 IP 地址?", L"监控服务", MB_ICONWARNING | MB_YESNO);
			if (ret == IDNO)
			{
				return FALSE;
			}
		}

		auto ok = start_service(ip);
		if (ok)
		{
			ok = save_ip(addr);
		}
		if (ok)
		{
			SetDlgItemText(IDC_STATUS, L"服务已启动!");
		}

		return TRUE;
	}

	LRESULT OnStopService(LPNMHDR pnmh)
	{
		auto stopped = stop_service();
		if (stopped)
		{
			SetDlgItemText(IDC_STATUS, L"服务已停止!");
		}
		return TRUE;
	}

private:
	void _Subclass()
	{
		mIP.Attach(GetDlgItem(IDC_REMOTEIP));
		mIP.SetAddress(MAKEIPADDRESS(0, 0, 0, 0));
		// mIP.SetAddress(MAKEIPADDRESS(192, 168, 2, 95));

		mInstallButton.SetBitmapButtonExtendedStyle(BMPBTN_HOVER, BMPBTN_HOVER);
		mInstallButtonImage.CreateFromImage(IDR_BMP_INSTALL, 90, 4, CLR_NONE, IMAGE_BITMAP, LR_CREATEDIBSECTION);
		mInstallButton.SetImageList(mInstallButtonImage);
		mInstallButton.SetImages(0, 1, 2, 3);
		mInstallButton.SubclassWindow(GetDlgItem(IDC_BTN_INSTALL));

		mUninstallButton.SetBitmapButtonExtendedStyle(BMPBTN_HOVER, BMPBTN_HOVER);
		mUninstallButtonImage.CreateFromImage(IDR_BMP_UNINSTALL, 90, 4, CLR_NONE, IMAGE_BITMAP, LR_CREATEDIBSECTION);
		mUninstallButton.SetImageList(mUninstallButtonImage);
		mUninstallButton.SetImages(0, 1, 2, 3);
		mUninstallButton.SubclassWindow(GetDlgItem(IDC_BTN_UNINSTALL));

		_SetButtonEnabled();
	}

	void _UnSubclass()
	{
		mIP.Detach();
		mInstallButton.UnsubclassWindow();
		mUninstallButton.UnsubclassWindow();
	}

	void _SetButtonEnabled()
	{
		mInstallButton.EnableWindow(mEnableInstall);
		mUninstallButton.EnableWindow(mEnableUninstall);
	}

	void _SetIcon(bool _small = false)
	{
		auto cx = !_small ? SM_CXICON : SM_CXSMICON;
		auto cy = !_small ? SM_CYICON : SM_CYSMICON;
		auto icon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(cx), ::GetSystemMetrics(cy));
		SetIcon(icon, True);
	}

	void _AddToMessageLoop()
	{
		auto _Loop = _AppModule.GetMessageLoop();
		ATLASSERT(!!_Loop);
		_Loop->AddMessageFilter(this);
		_Loop->AddIdleHandler(this);
	}

	void _RemoveFromMessageLoop()
	{
		auto _Loop = _AppModule.GetMessageLoop();
		ATLASSERT(!!_Loop);
		_Loop->RemoveIdleHandler(this);
		_Loop->RemoveMessageFilter(this);
	}

	void _Close()
	{
		DestroyWindow();
		PostQuitMessage(0);
	}

	void _CheckLinkEnable(bool enabled)
	{
		BOOL bEnable = enabled ? TRUE : FALSE;
		auto _start_link = GetDlgItem(IDC_SYSLINK_START);
		if (_start_link)
		{
			::EnableWindow(_start_link, bEnable);
		}
		auto _stop_link = GetDlgItem(IDC_SYSLINK_STOP);
		if (_stop_link)
		{
			::EnableWindow(_stop_link, bEnable);
		}
	}

	DWORD _IpAddress(wchar_t* buffer, int len)
	{
		DWORD addr;
		auto _ipfields = mIP.GetAddress(&addr);
		int ip0, ip1, ip2, ip3;
		if (_ipfields == 4)
		{
			ip0 = FIRST_IPADDRESS(addr);
			ip1 = SECOND_IPADDRESS(addr);
			ip2 = THIRD_IPADDRESS(addr);
			ip3 = FOURTH_IPADDRESS(addr);
		}
		else
		{
			return 0;
		}

		auto str_ip = std::format(L"{}.{}.{}.{}", ip0, ip1, ip2, ip3);
		auto err = wcscpy_s(buffer, len, str_ip.c_str());
		if (err != NO_ERROR)
		{
			buffer = nullptr;
			return 0;
		}
		return addr;
	}
};

#endif

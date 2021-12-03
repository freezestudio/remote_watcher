#ifndef MAINDLG_H
#define MAINDLG_H

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
		END_MSG_MAP()

public:
	BOOL OnInitDialog(CWindow, LPARAM)
	{
		CenterWindow();

		auto is_installed = is_service_installed();
		mEnableInstall = is_installed ? FALSE : TRUE;
		mEnableUninstall = is_installed ? TRUE : FALSE;
		SetDlgItemText(IDC_STATUS, is_installed ? L"installed!" : L"not installed!");

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
			//error
			return;
		}

		wchar_t ip[16]{};
		auto str_ip = std::format(L"{}.{}.{}.{}", ip0, ip1, ip2, ip3);
		wcscpy_s(ip, str_ip.c_str());
		if (install_service())
		{
			auto started = start_service(ip);
			auto _msg = std::format(L"Start Service Result: {}"sv, started);
			OutputDebugString(_msg.data());
			if (started)
			{
				mEnableInstall = FALSE;
				mEnableUninstall = TRUE;
			}
			else
			{
				mEnableInstall = TRUE;
				mEnableUninstall = FALSE;
			}
			_SetButtonEnabled();
		}
		else
		{
			OutputDebugString(L"Install Service Failure.\n");
			mEnableInstall = TRUE;
			mEnableUninstall = FALSE;
			_SetButtonEnabled();
		}
	}

	void OnUninstall(UINT uNotifyCode, int nID, CWindow /* wndCtl */)
	{
		if (stop_service())
		{
			auto uninstalled = uninstall_service();
			auto _msg = std::format(L"Uninstall Service Result: {}"sv, uninstalled);
			OutputDebugString(_msg.data());
			if (uninstalled)
			{
				mEnableInstall = TRUE;
				mEnableUninstall = FALSE;
			}
			else
			{
				mEnableInstall = FALSE;
				mEnableUninstall = TRUE;
			}
			_SetButtonEnabled();
		}
		else
		{
			OutputDebugString(L"Stop Service Failure.\n");
			mEnableInstall = TRUE;
			mEnableUninstall = FALSE;
			_SetButtonEnabled();
		}
	}

private:
	void _Subclass()
	{
		mIP.Attach(GetDlgItem(IDC_REMOTEIP));
		mIP.SetAddress(MAKEIPADDRESS(192, 168, 0, 1));

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
};

#endif

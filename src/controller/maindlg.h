#ifndef MAINDLG_H
#define MAINDLG_H

class MainDlg
    : public CDialogImpl<MainDlg>,
      public CUpdateUI<MainDlg>,
      public CMessageFilter,
      public CIdleHandler
{
public:
    // use in: CDialogImpl::Create(...)
    static constexpr auto IDD = IDD_MAINDLG;

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
    END_MSG_MAP()

public:
    BOOL OnInitDialog(CWindow, LPARAM)
    {
        CenterWindow();

        _SetIcon();
        _SetIcon(true);

        _AddMessage();

        UIAddChildWindowContainer(m_hWnd);

        return TRUE;
    }

    void OnDestroy()
    {
        _RemoveMessage();
    }

    void OnClose(UINT uNotifyCode, int nID, CWindow wndCtl)
    {
        _Close();
    }

private:
    void _SetIcon(bool _small = false)
    {
        auto cx = !_small ? SM_CXICON : SM_CXSMICON;
        auto cy = !_small ? SM_CYICON : SM_CYSMICON;
        auto icon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(cx), ::GetSystemMetrics(cy));
        SetIcon(icon, True);
    }

    void _AddMessage()
    {
        auto _Loop = _AppModule.GetMessageLoop();
        ATLASSERT(!!_Loop);
        _Loop->AddMessageFilter(this);
        _Loop->AddIdleHandler(this);
    }

    void _RemoveMessage()
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

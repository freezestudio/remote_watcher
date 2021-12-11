//
// service manager
//

#include "dep.h"
#include "assets.h"
#include "win32_dep.h"

#include "setup.h"
#include "maindlg.h"

WTL::CAppModule _AppModule;

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpstrCmdLine, int nCmdShow)
{
    auto hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ATLASSERT(SUCCEEDED(hr));

    AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES);

    hr = _AppModule.Init(nullptr, hInstance);
    ATLASSERT(SUCCEEDED(hr));

    CMessageLoop _Loop;
    _AppModule.AddMessageLoop(&_Loop);
    
    auto dlg{MainDlg()};
    if (dlg.Create(nullptr))
    {
        dlg.ShowWindow(nCmdShow);
    }
    else
    {
        ATLTRACE(L"Main Dialog creation failure!");
        return -1;
    }

    auto ret = _Loop.Run();

    _AppModule.RemoveMessageLoop();
    _AppModule.Term();
    ::CoUninitialize();
    return ret;
}

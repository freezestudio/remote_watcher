//
// Window Dependence Files
//
#ifndef WIN32_DEP_H
#define WIN32_DEP_H

#include <atlbase.h>
#include "atlapp.h"

extern WTL::CAppModule _AppModule;

#include <atlwin.h>
#include "atlframe.h"
#include "atlctrls.h"
#include "atlctrlx.h"
#include "atldlgs.h"
#include "atlcrack.h"
#include "atlmisc.h"

#define COMMCTL6_X64 "/manifestdependency:\" \
	type='win32' \
	name='Microsoft.Windows.Common-Controls' \
	version='6.0.0.0' \
	processorArchitecture='amd64' \
	publicKeyToken='6595b64144ccf1df' \
	language='*' \
\""
#pragma comment(linker, COMMCTL6_X64)

#endif

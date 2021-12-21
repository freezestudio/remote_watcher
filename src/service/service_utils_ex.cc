#include "service_utils_ex.h"

namespace freeze
{
	/////////////////////////////////////////////////////////////////////////////
	// reg_key - definitions

	reg_key::reg_key() noexcept :
		m_hKey(nullptr), m_samWOW64(0)
	{
	}

	reg_key::reg_key(_Inout_ reg_key& key) noexcept :
		m_hKey(nullptr)
	{
		REGSAM samWOW64 = key.m_samWOW64;
		Attach(key.Detach());
		m_samWOW64 = samWOW64;
	}

	reg_key::reg_key(_In_ HKEY hKey) noexcept :
		m_hKey(hKey), m_samWOW64(0)
	{
	}

	reg_key::~reg_key() noexcept
	{
		Close();
	}

	reg_key& reg_key::operator=(_Inout_ reg_key& key) noexcept
	{
		if (m_hKey != key.m_hKey)
		{
			Close();
			REGSAM samWOW64 = key.m_samWOW64;
			Attach(key.Detach());
			m_samWOW64 = samWOW64;
		}
		return(*this);
	}

	reg_key::operator HKEY() const noexcept
	{
		return m_hKey;
	}

	HKEY reg_key::Detach() noexcept
	{
		HKEY hKey = m_hKey;
		m_hKey = nullptr;
		m_samWOW64 = 0;
		return hKey;
	}

	void reg_key::Attach(_In_ HKEY hKey) noexcept
	{
		assert(m_hKey == nullptr);
		m_hKey = hKey;
		m_samWOW64 = 0;
	}

	LSTATUS reg_key::DeleteSubKey(_In_z_ LPCWSTR lpszSubKey) noexcept
	{
		assert(m_hKey != nullptr);

#if WINVER >= 0x0501
#ifdef _UNICODE
		static decltype(RegDeleteKeyExW)* pfnRegDeleteKeyEx = nullptr;
#else
		static decltype(RegDeleteKeyExA)* pfnRegDeleteKeyEx = nullptr;
#endif	// _UNICODE
		static bool bInitialized = false;

		if (!bInitialized)
		{
			HMODULE hAdvapi32 = GetModuleHandle(L"Advapi32.dll");
			if (hAdvapi32 != nullptr)
			{
#ifdef _UNICODE
				pfnRegDeleteKeyEx = (decltype(RegDeleteKeyExW)*)GetProcAddress(hAdvapi32, "RegDeleteKeyExW");
#else
				pfnRegDeleteKeyEx = (decltype(RegDeleteKeyExA)*)GetProcAddress(hAdvapi32, "RegDeleteKeyExA");
#endif	// _UNICODE
			}
			bInitialized = true;
		}

		if (pfnRegDeleteKeyEx != nullptr)
		{
			return pfnRegDeleteKeyEx(m_hKey, lpszSubKey, m_samWOW64, 0);
		}

#endif	// WINVER

		return RegDeleteKey(m_hKey, lpszSubKey);
	}

	LSTATUS reg_key::DeleteValue(_In_z_ LPCWSTR lpszValue) noexcept
	{
		assert(m_hKey != nullptr);
		return RegDeleteValue(m_hKey, (LPTSTR)lpszValue);
	}

	LSTATUS reg_key::Close() noexcept
	{
		LONG lRes = ERROR_SUCCESS;
		if (m_hKey != nullptr)
		{
			lRes = RegCloseKey(m_hKey);
			m_hKey = nullptr;
		}
		m_samWOW64 = 0;
		return lRes;
	}

	LSTATUS reg_key::Flush() noexcept
	{
		assert(m_hKey != nullptr);

		return ::RegFlushKey(m_hKey);
	}

	LSTATUS reg_key::EnumKey(
		_In_ DWORD iIndex,
		_Out_writes_to_(*pnNameLength, *pnNameLength) _Post_z_ LPTSTR pszName,
		_Inout_ LPDWORD pnNameLength,
		_Out_opt_ FILETIME* pftLastWriteTime) noexcept
	{
		FILETIME ftLastWriteTime;

		assert(m_hKey != nullptr);
		if (pftLastWriteTime == nullptr)
		{
			pftLastWriteTime = &ftLastWriteTime;
		}

		return ::RegEnumKeyEx(m_hKey, iIndex, pszName, pnNameLength, nullptr, nullptr, nullptr, pftLastWriteTime);
	}

	LSTATUS reg_key::NotifyChangeKeyValue(
		_In_ BOOL bWatchSubtree,
		_In_ DWORD dwNotifyFilter,
		_In_ HANDLE hEvent,
		_In_ BOOL bAsync) noexcept
	{
		assert(m_hKey != nullptr);
		assert((hEvent != nullptr) || !bAsync);

		return ::RegNotifyChangeKeyValue(m_hKey, bWatchSubtree, dwNotifyFilter, hEvent, bAsync);
	}

	LSTATUS reg_key::Create(
		_In_ HKEY hKeyParent,
		_In_z_ LPCWSTR lpszKeyName,
		_In_opt_z_ LPTSTR lpszClass,
		_In_ DWORD dwOptions,
		_In_ REGSAM samDesired,
		_In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr,
		_Out_opt_ LPDWORD lpdwDisposition) noexcept
	{
		assert(hKeyParent != nullptr);
		DWORD dw;
		HKEY hKey = nullptr;
		LONG lRes = RegCreateKeyEx(hKeyParent, lpszKeyName, 0, lpszClass, dwOptions, samDesired, lpSecAttr, &hKey, &dw);
		if (lRes == ERROR_SUCCESS)
		{
			if (lpdwDisposition != nullptr)
				*lpdwDisposition = dw;

			lRes = Close();
			m_hKey = hKey;
#if WINVER >= 0x0501
			m_samWOW64 = samDesired & (KEY_WOW64_32KEY | KEY_WOW64_64KEY);
#endif
		}
		return lRes;
	}

	LSTATUS reg_key::Open(
		_In_ HKEY hKeyParent,
		_In_opt_z_ LPCWSTR lpszKeyName,
		_In_ REGSAM samDesired) noexcept
	{
		assert(hKeyParent != nullptr);
		HKEY hKey = nullptr;
		LONG lRes = RegOpenKeyEx(hKeyParent, lpszKeyName, 0, samDesired, &hKey);
		if (lRes == ERROR_SUCCESS)
		{
			lRes = Close();
			assert(lRes == ERROR_SUCCESS);
			m_hKey = hKey;
#if WINVER >= 0x0501
			m_samWOW64 = samDesired & (KEY_WOW64_32KEY | KEY_WOW64_64KEY);
#endif
		}
		return lRes;
	}
	LSTATUS reg_key::QueryValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_opt_ DWORD* pdwType,
		_Out_opt_ void* pData,
		_Inout_ ULONG* pnBytes) throw()
	{
		assert(m_hKey != nullptr);
		return ::RegQueryValueEx(
			m_hKey,
			pszValueName,
			nullptr,
			pdwType,
			static_cast<LPBYTE>(pData),
			pnBytes
		);
	}

	LSTATUS reg_key::QueryDWORDValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_ DWORD& dwValue) noexcept
	{
		LONG lRes;
		ULONG nBytes;
		DWORD dwType;

		assert(m_hKey != nullptr);

		nBytes = sizeof(DWORD);
		lRes = ::RegQueryValueEx(m_hKey, pszValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue),
			&nBytes);
		if (lRes != ERROR_SUCCESS)
			return lRes;
		if (dwType != REG_DWORD)
			return ERROR_INVALID_DATA;

		return ERROR_SUCCESS;
	}
	LSTATUS reg_key::QueryQWORDValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_ ULONGLONG& qwValue) noexcept
	{
		LONG lRes;
		ULONG nBytes;
		DWORD dwType;

		assert(m_hKey != nullptr);

		nBytes = sizeof(ULONGLONG);
		lRes = ::RegQueryValueEx(m_hKey, pszValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&qwValue),
			&nBytes);
		if (lRes != ERROR_SUCCESS)
			return lRes;
		if (dwType != REG_QWORD)
			return ERROR_INVALID_DATA;

		return ERROR_SUCCESS;
	}

	LONG reg_key::QueryBinaryValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_opt_ void* pValue,
		_Inout_opt_ ULONG* pnBytes) noexcept
	{
		LONG lRes;
		DWORD dwType;

		assert(pnBytes != nullptr);
		assert(m_hKey != nullptr);

		lRes = ::RegQueryValueEx(m_hKey, pszValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(pValue),
			pnBytes);
		if (lRes != ERROR_SUCCESS)
			return lRes;
		if (dwType != REG_BINARY)
			return ERROR_INVALID_DATA;

		return ERROR_SUCCESS;
	}


	LSTATUS reg_key::QueryStringValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue,
		_Inout_ ULONG* pnChars) noexcept
	{
		LONG lRes;
		DWORD dwType;
		ULONG nBytes;

		assert(m_hKey != nullptr);
		assert(pnChars != nullptr);

		nBytes = (*pnChars) * sizeof(TCHAR);
		*pnChars = 0;
		lRes = ::RegQueryValueEx(
			m_hKey,
			pszValueName,
			nullptr,
			&dwType,
			reinterpret_cast<LPBYTE>(pszValue),
			&nBytes);

		if (lRes != ERROR_SUCCESS)
		{
			return lRes;
		}

		if (dwType != REG_SZ && dwType != REG_EXPAND_SZ)
		{
			return ERROR_INVALID_DATA;
		}

		if (pszValue != nullptr)
		{
			if (nBytes != 0)
			{
				if ((nBytes % sizeof(TCHAR) != 0) || (pszValue[nBytes / sizeof(TCHAR) - 1] != 0))
				{
					return ERROR_INVALID_DATA;
				}
			}
			else
			{
				pszValue[0] = L'\0';
			}
		}

		*pnChars = nBytes / sizeof(TCHAR);

		return ERROR_SUCCESS;
	}


	LSTATUS reg_key::QueryMultiStringValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue,
		_Inout_ ULONG* pnChars) noexcept
	{
		LONG lRes;
		DWORD dwType;
		ULONG nBytes;

		assert(m_hKey != nullptr);
		assert(pnChars != nullptr);

		if (pszValue != nullptr && *pnChars < 2)
			return ERROR_INSUFFICIENT_BUFFER;

		nBytes = (*pnChars) * sizeof(TCHAR);
		*pnChars = 0;

		lRes = ::RegQueryValueEx(
			m_hKey,
			pszValueName,
			nullptr,
			&dwType,
			reinterpret_cast<LPBYTE>(pszValue),
			&nBytes);
		if (lRes != ERROR_SUCCESS)
			return lRes;
		if (dwType != REG_MULTI_SZ)
			return ERROR_INVALID_DATA;
		if (pszValue != nullptr && (
			nBytes % sizeof(TCHAR) != 0 ||
			nBytes / sizeof(TCHAR) < 1 ||
			pszValue[nBytes / sizeof(TCHAR) - 1] != 0 ||
			((nBytes / sizeof(TCHAR)) > 1 && pszValue[nBytes / sizeof(TCHAR) - 2] != 0)))
		{
			return ERROR_INVALID_DATA;
		}
		*pnChars = nBytes / sizeof(TCHAR);

		return ERROR_SUCCESS;
	}

	LSTATUS reg_key::SetKeyValue(
		_In_z_ LPCWSTR lpszKeyName,
		_In_opt_z_ LPCWSTR lpszValue,
		_In_opt_z_ LPCWSTR lpszValueName) noexcept
	{
		assert(lpszValue != nullptr);
		reg_key key;
		LONG lRes = key.Create(m_hKey, lpszKeyName, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE | m_samWOW64);
		if (lRes == ERROR_SUCCESS)
			lRes = key.SetStringValue(lpszValueName, lpszValue);
		return lRes;
	}

	LSTATUS reg_key::SetValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_ DWORD dwType,
		_In_opt_ const void* pValue,
		_In_ ULONG nBytes) noexcept
	{
		assert(m_hKey != nullptr);
		return ::RegSetValueEx(
			m_hKey,
			pszValueName,
			0,
			dwType,
			static_cast<const BYTE*>(pValue), nBytes
		);
	}

	LSTATUS reg_key::SetBinaryValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_opt_ const void* pData,
		_In_ ULONG nBytes) noexcept
	{
		assert(m_hKey != nullptr);
		return ::RegSetValueEx(m_hKey, pszValueName, 0, REG_BINARY, reinterpret_cast<const BYTE*>(pData), nBytes);
	}

	LSTATUS reg_key::SetDWORDValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_ DWORD dwValue) noexcept
	{
		assert(m_hKey != nullptr);
		return ::RegSetValueEx(m_hKey, pszValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(DWORD));
	}

	LSTATUS reg_key::SetQWORDValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_ ULONGLONG qwValue) noexcept
	{
		assert(m_hKey != nullptr);
		return ::RegSetValueEx(m_hKey, pszValueName, 0, REG_QWORD, reinterpret_cast<const BYTE*>(&qwValue), sizeof(ULONGLONG));
	}

	LSTATUS reg_key::SetStringValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_opt_z_ LPCWSTR pszValue,
		_In_ DWORD dwType) noexcept
	{
		assert(m_hKey != nullptr);
		assert(pszValue != nullptr/*, ERROR_INVALID_DATA*/);
		assert((dwType == REG_SZ) || (dwType == REG_EXPAND_SZ));

		return ::RegSetValueEx(
			m_hKey,
			pszValueName,
			0,
			dwType,
			reinterpret_cast<const BYTE*>(pszValue),
			(static_cast<DWORD>(wcslen(pszValue)) + 1) * sizeof(TCHAR)
		);
	}

	LSTATUS reg_key::SetMultiStringValue(
		_In_opt_z_ LPCWSTR pszValueName,
		_In_z_ LPCWSTR pszValue) noexcept
	{
		LPCWSTR pszTemp;
		ULONG nBytes;
		ULONG nLength;

		assert(m_hKey != nullptr);
		assert(pszValue != nullptr/*, ERROR_INVALID_DATA*/);

		// Find the total length (in bytes) of all of the strings, including the
		// terminating '\0' of each string, and the second '\0' that terminates
		// the list.
		nBytes = 0;
		pszTemp = pszValue;
		do
		{
			nLength = static_cast<ULONG>(wcslen(pszTemp)) + 1;
			pszTemp += nLength;
			nBytes += nLength * sizeof(TCHAR);
		} while (nLength != 1);

		return ::RegSetValueEx(m_hKey, pszValueName, 0, REG_MULTI_SZ, reinterpret_cast<const BYTE*>(pszValue),
			nBytes);
	}

	LSTATUS reg_key::GetKeySecurity(
		_In_ SECURITY_INFORMATION si,
		_Out_opt_ PSECURITY_DESCRIPTOR psd,
		_Inout_ LPDWORD pnBytes) noexcept
	{
		assert(m_hKey != nullptr);
		assert(pnBytes != nullptr);

		return ::RegGetKeySecurity(m_hKey, si, psd, pnBytes);
	}

	LSTATUS reg_key::SetKeySecurity(
		_In_ SECURITY_INFORMATION si,
		_In_ PSECURITY_DESCRIPTOR psd) noexcept
	{
		assert(m_hKey != nullptr);
		assert(psd != nullptr);

		return ::RegSetKeySecurity(m_hKey, si, psd);
	}

	LSTATUS reg_key::RecurseDeleteKey(_In_z_ LPCWSTR lpszKey) noexcept
	{
		reg_key key;
		LONG lRes = key.Open(m_hKey, lpszKey, KEY_READ | KEY_WRITE | m_samWOW64);
		if (lRes != ERROR_SUCCESS)
		{
			if (lRes != ERROR_FILE_NOT_FOUND && lRes != ERROR_PATH_NOT_FOUND)
			{
				//TRACE(reg_key, 0, _T("reg_key::RecurseDeleteKey : Failed to Open Key %Ts(Error = %d)\n"), lpszKey, lRes);
			}
			return lRes;
		}
		FILETIME time;
		DWORD dwSize = 256;
		TCHAR szBuffer[256];
		while (RegEnumKeyEx(key.m_hKey, 0, szBuffer, &dwSize, nullptr, nullptr, nullptr,
			&time) == ERROR_SUCCESS)
		{
			lRes = key.RecurseDeleteKey(szBuffer);
			if (lRes != ERROR_SUCCESS)
				return lRes;
			dwSize = 256;
		}
		key.Close();
		return DeleteSubKey(lpszKey);
	}
}

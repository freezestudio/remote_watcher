#ifndef SERVICE_UTILS_EX_H
#define SERVICE_UTILS_EX_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>

namespace freeze
{
	class reg_key
	{
	public:
		reg_key() noexcept;
		reg_key(_Inout_ reg_key& key) noexcept;
		explicit reg_key(_In_ HKEY hKey) noexcept;
		~reg_key() noexcept;

		reg_key& operator=(_Inout_ reg_key& key) noexcept;

		// Attributes
	public:
		operator HKEY() const noexcept;
		HKEY m_hKey;
		REGSAM m_samWOW64;

		// Operations
	public:
		LSTATUS SetValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_ DWORD dwType,
			_In_opt_ const void* pValue,
			_In_ ULONG nBytes) noexcept;

		LSTATUS SetBinaryValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_opt_ const void* pValue,
			_In_ ULONG nBytes) noexcept;

		LSTATUS SetDWORDValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_ DWORD dwValue) noexcept;

		LSTATUS SetQWORDValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_ ULONGLONG qwValue) noexcept;

		LSTATUS SetStringValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_opt_z_ LPCWSTR pszValue,
			_In_ DWORD dwType = REG_SZ) noexcept;

		LSTATUS SetMultiStringValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_In_z_ LPCWSTR pszValue) noexcept;

		LSTATUS QueryValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_opt_ DWORD* pdwType,
			_Out_opt_ void* pData,
			_Inout_ ULONG* pnBytes) noexcept;


		LSTATUS QueryBinaryValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_opt_ void* pValue,
			_Inout_opt_ ULONG* pnBytes) noexcept;

		LSTATUS QueryDWORDValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_ DWORD& dwValue) noexcept;

		LSTATUS QueryQWORDValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_ ULONGLONG& qwValue) noexcept;

		LSTATUS QueryStringValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue,
			_Inout_ ULONG* pnChars) noexcept;

		LSTATUS QueryMultiStringValue(
			_In_opt_z_ LPCWSTR pszValueName,
			_Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue,
			_Inout_ ULONG* pnChars) noexcept;

		// Get the key's security attributes.
		LSTATUS GetKeySecurity(
			_In_ SECURITY_INFORMATION si,
			_Out_opt_ PSECURITY_DESCRIPTOR psd,
			_Inout_ LPDWORD pnBytes) noexcept;
		// Set the key's security attributes.
		LSTATUS SetKeySecurity(
			_In_ SECURITY_INFORMATION si,
			_In_ PSECURITY_DESCRIPTOR psd) noexcept;

		LSTATUS SetKeyValue(
			_In_z_ LPCWSTR lpszKeyName,
			_In_opt_z_ LPCWSTR lpszValue,
			_In_opt_z_ LPCWSTR lpszValueName = nullptr) noexcept;

		static LSTATUS WINAPI SetValue(
			_In_ HKEY hKeyParent,
			_In_z_ LPCWSTR lpszKeyName,
			_In_opt_z_ LPCWSTR lpszValue,
			_In_opt_z_ LPCWSTR lpszValueName = nullptr);

		// Create a new registry key (or open an existing one).
		LSTATUS Create(
			_In_ HKEY hKeyParent,
			_In_z_ LPCWSTR lpszKeyName,
			_In_opt_z_ LPTSTR lpszClass = REG_NONE,
			_In_ DWORD dwOptions = REG_OPTION_NON_VOLATILE,
			_In_ REGSAM samDesired = KEY_READ | KEY_WRITE,
			_In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr = nullptr,
			_Out_opt_ LPDWORD lpdwDisposition = nullptr) noexcept;
		// Open an existing registry key.
		LSTATUS Open(
			_In_ HKEY hKeyParent,
			_In_opt_z_ LPCWSTR lpszKeyName,
			_In_ REGSAM samDesired = KEY_READ | KEY_WRITE) noexcept;
		// Close the registry key.
		LSTATUS Close() noexcept;
		// Flush the key's data to disk.
		LSTATUS Flush() noexcept;

		// Detach the reg_key object from its HKEY.  Releases ownership.
		HKEY Detach() noexcept;
		// Attach the reg_key object to an existing HKEY.  Takes ownership.
		void Attach(_In_ HKEY hKey) noexcept;

		// Enumerate the subkeys of the key.
		LSTATUS EnumKey(
			_In_ DWORD iIndex,
			_Out_writes_to_(*pnNameLength, *pnNameLength) _Post_z_ LPTSTR pszName,
			_Inout_ LPDWORD pnNameLength,
			_Out_opt_ FILETIME* pftLastWriteTime = nullptr) noexcept;
		LSTATUS NotifyChangeKeyValue(
			_In_ BOOL bWatchSubtree,
			_In_ DWORD dwNotifyFilter,
			_In_ HANDLE hEvent,
			_In_ BOOL bAsync = TRUE) noexcept;

		LSTATUS DeleteSubKey(_In_z_ LPCWSTR lpszSubKey) noexcept;
		LSTATUS RecurseDeleteKey(_In_z_ LPCWSTR lpszKey) noexcept;
		LSTATUS DeleteValue(_In_z_ LPCWSTR lpszValue) noexcept;
	};
}

#endif
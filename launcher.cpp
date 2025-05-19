#include "launcher.h"
#include <sddl.h>
#include <wtsapi32.h>

#pragma comment(lib, "wtsapi32.lib")


#define PROCESS_ID_UPPER_LIMIT  65536
#define MAX_ACCESS_TOKEN_LENGTH 4096

bool SetPrivilege(HANDLE hToken, LPCWSTR szPrivilege)
{
	LUID luid = { 0 };
	if (!LookupPrivilegeValue(NULL, szPrivilege, &luid))
		return false;
	TOKEN_PRIVILEGES tp = { 0 };
	ZeroMemory(&tp, sizeof(tp));
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	return AdjustTokenPrivileges(hToken, FALSE,
		&tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) ? true : false;
}

HANDLE GetTokenForLocalSystem()
{
	HANDLE hCurrentToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hCurrentToken)) {
		return NULL;
	}
	SetPrivilege(hCurrentToken, SE_DEBUG_NAME);
	for (DWORD pid = 0; pid < PROCESS_ID_UPPER_LIMIT; pid += 4)
	{
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (hProcess)
		{
			HANDLE hToken = 0;
			if (OpenProcessToken(hProcess,
				TOKEN_QUERY | TOKEN_READ | TOKEN_IMPERSONATE | TOKEN_QUERY_SOURCE |
				TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_EXECUTE, &hToken))
			{
				char tokenBuf[MAX_ACCESS_TOKEN_LENGTH];
				TOKEN_USER* pUserToken = (TOKEN_USER*)tokenBuf;
				DWORD dwRetLength = 0;
				if (GetTokenInformation(hToken, TokenUser, pUserToken, sizeof(tokenBuf), &dwRetLength)) {
					WCHAR* pSidString = NULL;
					if (ConvertSidToStringSid(pUserToken->User.Sid, &pSidString) && pSidString) {
						if (wcscmp(pSidString, L"S-1-5-18") == 0) // Sid of LocalSystem
						{
							// found the token for LocalSystem
							CloseHandle(hProcess);
							LocalFree(pSidString);
							return hToken;
						}
						LocalFree(pSidString);
					}
				}
				CloseHandle(hToken);
			}
			CloseHandle(hProcess);
		}
	}
	return NULL;
}

bool MakeInteractive(HANDLE hToken, DWORD *pdwOldSessionId) {
	PWTS_SESSION_INFO psi;
	DWORD dwCount = 0;
	if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &psi, &dwCount)) {
		for (int i = 0; i < (int)dwCount; i++) {
			if (psi[i].State == WTSActive) {
				DWORD dwRetLength = 0;
				GetTokenInformation(hToken, TokenSessionId, pdwOldSessionId, sizeof(*pdwOldSessionId), &dwRetLength);
				SetPrivilege(hToken, SE_TCB_NAME);
				SetTokenInformation(hToken, TokenSessionId, &psi[i].SessionId, sizeof(psi[i].SessionId));
				WTSFreeMemory(psi);
				return true;
			}
		}
		WTSFreeMemory(psi);
	}
	return false;
}

// launch the interactive process using local system account
bool Launch(LPCWSTR szFilePath) {
	// get LocalSystem token
	HANDLE hLocalSystemToken = GetTokenForLocalSystem();
	if (hLocalSystemToken == NULL)
		return false;
	HANDLE hDupLocalSystemToken = NULL;
	bool ret = DuplicateTokenEx(hLocalSystemToken, MAXIMUM_ALLOWED, NULL,
		SecurityImpersonation, TokenPrimary, &hDupLocalSystemToken);
	CloseHandle(hLocalSystemToken);
	if (!ret)
		return false;
	// make LocalSystem token interactive
	DWORD oldSessionId = 0;
	ret = MakeInteractive(hDupLocalSystemToken, &oldSessionId);
	if (!ret) {
		CloseHandle(hDupLocalSystemToken);
		return false;
	}
	// launch the process
	WCHAR szDesktop[MAX_PATH] = { L"WinSta0\\Default" };
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.lpDesktop = szDesktop;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	WCHAR szProcFilePath[MAX_PATH] = { 0 };
	wcscpy_s(szProcFilePath, szFilePath);
	ret = CreateProcessAsUser(hDupLocalSystemToken, NULL, szProcFilePath,
		NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	CloseHandle(hDupLocalSystemToken);
	return ret;
}

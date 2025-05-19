#include <windows.h>
#include <shlwapi.h>
#include "launcher.h"
#include <iostream>

#pragma comment(lib, "shlwapi.lib")


#define SERVICE_NAME_PREFIX     L"Sudo"
#define SERVICE_NAME_MAX_LENGTH 256
#define MAX_CMD_LENGTH          1024
#define WAIT_FOR_EXECUTION_TIME_MILLISECONDS 2000

WCHAR g_szServiceName[SERVICE_NAME_MAX_LENGTH] = { 0 };

VOID WINAPI ServiceCtrlHandler(DWORD);

void GetServiceName();
void Install(LPWSTR szTargetExePath);
void Start();
void Delete();

void ShowHelp();
bool IsFileValid(LPCWSTR exePath);

int wmain(int argc, TCHAR* argv[]) {
    if (argc >= 3 && wcscmp(argv[1], L"-e") == 0 && IsFileValid(argv[2])) {
        Launch(argv[2]);
        return 0;
    }
    else if (argc == 2 && IsFileValid(argv[1])) {
        Install(argv[1]);
        Start();
        Sleep(WAIT_FOR_EXECUTION_TIME_MILLISECONDS);
        Delete();
        return 0;
    }
    else {
        ShowHelp();
    }
    return 0;
}

void GetServiceName() {
    std::srand((unsigned)std::time(nullptr));
    int randomSuffix = std::rand() % 10000;
    swprintf(g_szServiceName, SERVICE_NAME_MAX_LENGTH, L"%s%d", SERVICE_NAME_PREFIX, randomSuffix);
}

void Install(LPWSTR szTargetExePath){
    GetServiceName();
    TCHAR szCmd[MAX_CMD_LENGTH] = { 0 };
    GetModuleFileName(NULL, szCmd, MAX_PATH);
    wcscat_s(szCmd, L" -e ");
    wcscat_s(szCmd, szTargetExePath);
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return;
    SC_HANDLE svc = CreateService(
        scm, g_szServiceName, g_szServiceName,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        szCmd, NULL, NULL, NULL, NULL, NULL);
    if (svc) {
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

void Start() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenService(scm, g_szServiceName, SERVICE_START);
    if (svc) {
        StartService(svc, 0, NULL);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

void Delete() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenService(scm, g_szServiceName, DELETE);
    if (svc) {
        DeleteService(svc);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

void ShowHelp() {
    puts("sudo absolute_path_of_exe");
}

bool IsFileValid(LPCWSTR exePath) {
    return PathFileExists(exePath) ? true : false;
}

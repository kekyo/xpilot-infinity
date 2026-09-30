#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include "service_paths_win32.h"
#include <stdlib.h>
#include <wchar.h>

/* The broker performs both the potentially blocking SCM call and any UAC
 * interaction outside the desktop event loop. Its lifetime is independent
 * of the tray. No arbitrary service, executable or command is accepted. */
int wmain(int argc, wchar_t **argv)
{
    if ((argc != 2 && argc != 3) || (wcscmp(argv[1], L"--start") && wcscmp(argv[1], L"--stop"))
        || (argc == 3 && wcscmp(argv[2], L"--authorized")))
        return ERROR_INVALID_PARAMETER;
    bool start = !wcscmp(argv[1], L"--start");
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    DWORD error = manager ? ERROR_SUCCESS : GetLastError();
    SC_HANDLE service = manager ? OpenServiceW(manager, L"XPilotInfinityServer",
        start ? SERVICE_START : SERVICE_STOP) : NULL;
    if (manager && !service) error = GetLastError();
    if (manager) CloseServiceHandle(manager);
    if (service) {
        SERVICE_STATUS status;
        if (!(start ? StartServiceW(service, 0, NULL)
                    : ControlService(service, SERVICE_CONTROL_STOP, &status)))
            error = GetLastError();
        CloseServiceHandle(service);
    }
    if (error != ERROR_ACCESS_DENIED || argc == 3)
        return (int)error;
    wchar_t *path = NULL;
    error = service_helper_path_win32(&path);
    if (error) return (int)error;
    SHELLEXECUTEINFOW launch = {0};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    launch.lpVerb = L"runas";
    launch.lpFile = path;
    launch.lpParameters = start ? L"--start --authorized" : L"--stop --authorized";
    launch.nShow = SW_HIDE;
    if (!ShellExecuteExW(&launch)) error = GetLastError();
    else if (!launch.hProcess) error = ERROR_INVALID_HANDLE;
    else {
        WaitForSingleObject(launch.hProcess, INFINITE);
        if (!GetExitCodeProcess(launch.hProcess, &error)) error = GetLastError();
        CloseHandle(launch.hProcess);
    }
    free(path);
    return (int)error;
}

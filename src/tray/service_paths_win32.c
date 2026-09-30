#include "service_paths_win32.h"
#include <shellapi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

DWORD service_paths_win32(wchar_t **directory, wchar_t **configuration)
{
    *directory = *configuration = NULL;
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager) return GetLastError();
    SC_HANDLE service = OpenServiceW(manager, L"XPilotInfinityServer", SERVICE_QUERY_CONFIG);
    DWORD error = service ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(manager);
    if (!service) return error;
    DWORD needed = 0;
    QueryServiceConfigW(service, NULL, 0, &needed);
    QUERY_SERVICE_CONFIGW *config = malloc(needed);
    if (!config) { CloseServiceHandle(service); return ERROR_NOT_ENOUGH_MEMORY; }
    if (!QueryServiceConfigW(service, config, needed, &needed)) {
        error = GetLastError();
        free(config); CloseServiceHandle(service); return error;
    }
    CloseServiceHandle(service);
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(config->lpBinaryPathName, &count);
    free(config);
    if (!arguments) return GetLastError();
    const wchar_t *separator = count ? wcsrchr(arguments[0], L'\\') : NULL;
    if (!separator || _wcsicmp(separator + 1, L"xpilot-infinity-server.exe")
        || wcslen(arguments[0]) < 3 || arguments[0][1] != L':' || arguments[0][2] != L'\\') {
        LocalFree(arguments); return ERROR_BAD_CONFIGURATION;
    }
    size_t length = (size_t)(separator - arguments[0]);
    *directory = calloc(length + 1, sizeof(wchar_t));
    if (*directory) wmemcpy(*directory, arguments[0], length);
    bool service_mode = false;
    for (int i = 1; i < count; i++) {
        if (!wcscmp(arguments[i], L"--windows-service")) service_mode = true;
        if (!wcscmp(arguments[i], L"-defaultsFileName") && i + 1 < count) {
            if (*configuration) { error = ERROR_BAD_CONFIGURATION; break; }
            *configuration = _wcsdup(arguments[++i]);
        }
    }
    LocalFree(arguments);
    if (!*directory || !*configuration) error = ERROR_BAD_CONFIGURATION;
    if (!service_mode) error = ERROR_BAD_CONFIGURATION;
    if (error) {
        free(*directory); free(*configuration);
        *directory = *configuration = NULL;
    }
    return error;
}

DWORD service_helper_path_win32(wchar_t **path)
{
    wchar_t *directory, *configuration;
    DWORD error = service_paths_win32(&directory, &configuration);
    *path = NULL;
    if (error) return error;
    const wchar_t suffix[] = L"\\xpilot-infinity-service-helper.exe";
    size_t length = wcslen(directory) + wcslen(suffix) + 1;
    *path = malloc(length * sizeof(wchar_t));
    if (*path) swprintf(*path, length, L"%ls%ls", directory, suffix);
    else error = ERROR_NOT_ENOUGH_MEMORY;
    free(directory); free(configuration);
    return error;
}

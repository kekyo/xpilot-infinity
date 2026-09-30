#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include "service_paths_win32.h"
#include "settings_file_win32.h"
#include "settings_transfer_win32.h"
#include "service_config.h"
#include "service_native.h"
#include "tray_controller.h"
#include "utf8_files.h"
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

/* A service request has finished only after its target state is observed. */
static DWORD transition(service_native *native, tray_controller *controller, service_action action)
{
    if (!tray_controller_request(controller, action)) return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    ULONGLONG deadline = GetTickCount64() + 180000;
    while (tray_controller_status(controller)->busy) {
        ULONGLONG now = GetTickCount64();
        if (now >= deadline) return ERROR_SERVICE_REQUEST_TIMEOUT;
        service_native_dispatch(native, (unsigned)(deadline - now));
    }
    const tray_status *status = tray_controller_status(controller);
    if (status->operation_error != XP_SERVICE_OK) return ERROR_SERVICE_REQUEST_TIMEOUT;
    service_state target = action == XP_SERVICE_START ? XP_SERVICE_RUNNING : XP_SERVICE_STOPPED;
    return status->service.state == target ? ERROR_SUCCESS : ERROR_SERVICE_NOT_ACTIVE;
}

static DWORD save_configuration(const char *generation, const char *map, const char *text)
{
    wchar_t *directory = NULL, *path = NULL;
    DWORD error = service_paths_win32(&directory, &path);
    if (error) { free(directory); free(path); return error; }
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE service = manager ? OpenServiceW(manager, L"XPilotInfinityServer",
        SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG) : NULL;
    if (!service) error = GetLastError();
    if (manager) CloseServiceHandle(manager);
    SERVICE_STATUS_PROCESS before = {0};
    DWORD needed;
    if (!error && !QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        (BYTE *)&before, sizeof(before), &needed)) error = GetLastError();
    bool running = before.dwCurrentState == SERVICE_RUNNING;
    if (!error && !running && before.dwCurrentState != SERVICE_STOPPED) error = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    if (!error && running) {
        /* Preflight both mutation rights before saving, so UAC occurs once. */
        manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        SC_HANDLE rights = manager ? OpenServiceW(manager, L"XPilotInfinityServer", SERVICE_START | SERVICE_STOP) : NULL;
        if (!rights) error = GetLastError();
        if (rights) CloseServiceHandle(rights);
        if (manager) CloseServiceHandle(manager);
        if (!(before.dwControlsAccepted & SERVICE_ACCEPT_STOP)) error = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
        QueryServiceConfigW(service, NULL, 0, &needed);
        QUERY_SERVICE_CONFIGW *config = malloc(needed);
        if (!config) error = ERROR_NOT_ENOUGH_MEMORY;
        else if (!QueryServiceConfigW(service, config, needed, &needed)) error = GetLastError();
        else if (config->dwStartType == SERVICE_DISABLED) error = ERROR_SERVICE_DISABLED;
        free(config);
    }
    char *selected = NULL;
    if (!error && !text) error = settings_win32_map(map, &selected);
    settings_file_win32 *file = !error ? settings_win32_open(generation, &error) : NULL;
    char *replacement = file && !text ? service_config_select_map(settings_win32_text(file), selected, false) : NULL;
    if (file && text) {
        replacement = malloc(strlen(text) + 1);
        if (replacement) memcpy(replacement, text, strlen(text) + 1);
    }
    if (file && !replacement) error = ERROR_BAD_CONFIGURATION;
    SERVICE_STATUS_PROCESS current;
    if (!error && !QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        (BYTE *)&current, sizeof(current), &needed)) error = GetLastError();
    if (!error && (current.dwCurrentState != before.dwCurrentState
        || current.dwProcessId != before.dwProcessId)) error = ERROR_REVISION_MISMATCH;
    wchar_t *check_directory = NULL, *check_path = NULL;
    if (!error) error = service_paths_win32(&check_directory, &check_path);
    if (!error && (wcscmp(path, check_path) || wcscmp(directory, check_directory))) error = ERROR_REVISION_MISMATCH;
    free(check_directory); free(check_path);
    bool changed = !error && strcmp(replacement, settings_win32_text(file));
    bool saved = false;
    char saved_generation[65] = "";
    if (changed) {
        error = settings_win32_generation(replacement, saved_generation);
        if (!error) error = settings_win32_record(file, saved_generation,
            "Configuration save/restart is pending or was interrupted; active settings are unverified");
        if (!error) { error = settings_win32_commit(file, replacement); saved = !error; }
    }
    /* Keep the configuration lock until the whole operation completes. */
    if (!error && changed && running) {
        service_native *native = service_native_create();
        tray_controller *controller = native ? tray_controller_create(service_native_control(native)) : NULL;
        if (!controller) error = ERROR_NOT_ENOUGH_MEMORY;
        else {
            tray_controller_connect(controller);
            if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (BYTE *)&current, sizeof(current), &needed))
                error = GetLastError();
            else if (current.dwCurrentState != SERVICE_RUNNING || current.dwProcessId != before.dwProcessId)
                error = ERROR_REVISION_MISMATCH;
            if (!error) error = transition(native, controller, XP_SERVICE_STOP);
            char *actual = NULL, actual_generation[65], expected_generation[65];
            if (!error) error = settings_win32_read(path, &actual, actual_generation);
            if (!error) error = settings_win32_generation(replacement, expected_generation);
            if (!error && strcmp(actual_generation, expected_generation)) error = ERROR_REVISION_MISMATCH;
            free(actual);
            if (!error) error = transition(native, controller, XP_SERVICE_START);
        }
        tray_controller_destroy(controller);
        service_native_destroy(native);
    }
    if (saved) settings_win32_record(file, saved_generation,
        error ? "Configuration saved; service restart failed or was interrupted. Active settings are unverified."
        : running ? "Configuration saved; service running. Game readiness and the active map remain unverified."
        : "Configuration saved; service remains stopped.");
    settings_win32_close(file);
    if (service) CloseServiceHandle(service);
    free(selected); free(replacement); free(path); free(directory);
    return saved && error ? error | 0x20000000UL : error;
}

static DWORD service_action_request(bool start)
{
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    DWORD error = manager ? ERROR_SUCCESS : GetLastError();
    SC_HANDLE service = manager ? OpenServiceW(manager, L"XPilotInfinityServer",
        start ? SERVICE_START : SERVICE_STOP) : NULL;
    if (manager && !service) error = GetLastError();
    if (manager) CloseServiceHandle(manager);
    if (service) {
        SERVICE_STATUS status;
        if (!(start ? StartServiceW(service, 0, NULL)
                    : ControlService(service, SERVICE_CONTROL_STOP, &status))) error = GetLastError();
        CloseServiceHandle(service);
    }
    return error;
}

/* The broker performs both SCM calls and UAC outside the desktop event loop.
 * Accepted work is independent of the tray. Only fixed service operations and
 * a generation plus a map ID or a hashed snapshot object are accepted, never
 * filesystem paths or arbitrary commands. Snapshot handles survive UI exit. */
int wmain(int argc, wchar_t **argv)
{
    bool authorized = argc > 1 && !wcscmp(argv[argc - 1], L"--authorized");
    int count = argc - (authorized ? 1 : 0);
    bool map = count == 4 && !wcscmp(argv[1], L"--map");
    bool edit = count == 5 && !wcscmp(argv[1], L"--apply");
    bool action = count == 2 && (!wcscmp(argv[1], L"--start") || !wcscmp(argv[1], L"--stop"));
    if (!map && !edit && !action) return ERROR_INVALID_PARAMETER;
    char *generation = NULL, *name = NULL;
    if (map || edit) generation = Xp_utf8(argv[2]);
    if ((map || edit) && (!generation || strlen(generation) != 64
        || strspn(generation, "0123456789abcdef") != 64)) { free(generation); return ERROR_INVALID_PARAMETER; }
    if (map) {
        size_t length = wcslen(argv[3]);
        if (!generation || strlen(generation) != 64 || strspn(generation, "0123456789abcdef") != 64
            || !length || length > 8192 || length % 2) { free(generation); return ERROR_INVALID_PARAMETER; }
        name = calloc(length / 2 + 1, 1);
        if (!name) { free(generation); return ERROR_NOT_ENOUGH_MEMORY; }
        const wchar_t digits[] = L"0123456789abcdef";
        for (size_t i = 0; i < length; i++) {
            const wchar_t *digit = wcschr(digits, argv[3][i]);
            if (!digit) { free(generation); free(name); return ERROR_INVALID_PARAMETER; }
            name[i / 2] = (char)((unsigned char)name[i / 2] * 16 + (digit - digits));
        }
        if (strlen(name) != length / 2) { free(generation); free(name); return ERROR_INVALID_PARAMETER; }
    }
    char *snapshot = NULL, *transfer = NULL, *hash = NULL;
    HANDLE mapping = NULL;
    DWORD error = ERROR_SUCCESS;
    if (edit) {
        transfer = Xp_utf8(argv[3]); hash = Xp_utf8(argv[4]);
        error = transfer && hash ? settings_transfer_read(transfer, hash, &snapshot, &mapping) : ERROR_INVALID_PARAMETER;
    }
    if (!error) error = map || edit ? save_configuration(generation, name, snapshot)
        : service_action_request(!wcscmp(argv[1], L"--start"));
    free(snapshot); free(transfer); free(hash);
    free(generation); free(name);
    if (error != ERROR_ACCESS_DENIED || authorized) { if (mapping) CloseHandle(mapping); return (int)error; }
    wchar_t *path = NULL;
    error = service_helper_path_win32(&path);
    if (error) { if (mapping) CloseHandle(mapping); return (int)error; }
    size_t length = edit ? 256 : map ? wcslen(argv[2]) + wcslen(argv[3]) + 48 : 48;
    wchar_t *parameters = calloc(length, sizeof(wchar_t));
    if (!parameters) { free(path); if (mapping) CloseHandle(mapping); return ERROR_NOT_ENOUGH_MEMORY; }
    if (edit) swprintf(parameters, length, L"--apply %ls %ls %ls --authorized", argv[2], argv[3], argv[4]);
    else if (map) swprintf(parameters, length, L"--map %ls %ls --authorized", argv[2], argv[3]);
    else swprintf(parameters, length, L"%ls --authorized", argv[1]);
    SHELLEXECUTEINFOW launch = {0};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    launch.lpVerb = L"runas";
    launch.lpFile = path;
    launch.lpParameters = parameters;
    launch.nShow = SW_HIDE;
    if (!ShellExecuteExW(&launch)) error = GetLastError();
    else if (!launch.hProcess) error = ERROR_INVALID_HANDLE;
    else {
        WaitForSingleObject(launch.hProcess, INFINITE);
        if (!GetExitCodeProcess(launch.hProcess, &error)) error = GetLastError();
        CloseHandle(launch.hProcess);
    }
    free(path); free(parameters);
    if (mapping) CloseHandle(mapping);
    return (int)error;
}

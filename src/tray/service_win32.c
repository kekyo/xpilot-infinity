#define _WIN32_WINNT 0x0600
#include <windows.h>
#include "service_native.h"
#include "service_paths_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRODUCT_SERVICE L"XPilotInfinityServer"
#define STATE_NOTIFICATIONS (SERVICE_NOTIFY_STOPPED | SERVICE_NOTIFY_START_PENDING \
    | SERVICE_NOTIFY_STOP_PENDING | SERVICE_NOTIFY_RUNNING | SERVICE_NOTIFY_PAUSED \
    | SERVICE_NOTIFY_PAUSE_PENDING | SERVICE_NOTIFY_CONTINUE_PENDING \
    | SERVICE_NOTIFY_DELETE_PENDING)

struct service_native {
    service_receiver receiver;
    uint64_t generation;
    uint64_t revision;
    SC_HANDLE manager;
    SC_HANDLE service;
    SERVICE_NOTIFYW status_notice;
    SERVICE_NOTIFYW manager_notice;
    bool attached;
    bool status_ready;
    bool manager_ready;
    bool status_armed;
    bool manager_armed;
    bool authorize;
    HANDLE helper;
    HANDLE extra_wait[4];
    unsigned extra_count;
    HWND dialog;
    ULONGLONG retry_at;
    unsigned retry_delay;
    uint64_t operation;
};

static service_error classify_error(DWORD code)
{
    if (code == ERROR_ACCESS_DENIED)
        return XP_SERVICE_FORBIDDEN;
    if (code == ERROR_CANCELLED)
        return XP_SERVICE_CANCELLED;
    if (code == ERROR_SERVICE_DISABLED)
        return XP_SERVICE_DISABLED;
    if (code == RPC_S_SERVER_UNAVAILABLE)
        return XP_SERVICE_UNAVAILABLE;
    return XP_SERVICE_ERROR;
}

static void describe_error(DWORD code, char *buffer, size_t size)
{
    wchar_t message[256] = {0};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, code, 0, message, 256, NULL);
    if (!WideCharToMultiByte(CP_UTF8, 0, message, -1, buffer, (int)size, NULL, NULL))
        snprintf(buffer, size, "Windows service error %lu", (unsigned long)code);
}

static void publish(service_native *native, const service_snapshot *snapshot)
{
    if (native->attached)
        native->receiver.observe(native->receiver.context, native->generation,
                                 ++native->revision, snapshot);
}

static void publish_error(service_native *native, DWORD code)
{
    service_snapshot snapshot = {XP_SERVICE_UNKNOWN, classify_error(code), false, false, ""};
    if (code == ERROR_SERVICE_DOES_NOT_EXIST || code == ERROR_SERVICE_MARKED_FOR_DELETE) {
        snapshot.state = XP_SERVICE_NOT_INSTALLED;
        snapshot.error = XP_SERVICE_OK;
    }
    describe_error(code, snapshot.detail, sizeof(snapshot.detail));
    publish(native, &snapshot);
    if (native->attached && snapshot.state != XP_SERVICE_NOT_INSTALLED && !native->retry_at) {
        native->retry_delay = native->retry_delay ? native->retry_delay * 2 : 250;
        if (native->retry_delay > 30000) native->retry_delay = 30000;
        native->retry_at = GetTickCount64() + native->retry_delay;
    }
}

/* SCM callbacks are APCs. Only record completion here: RPC calls (including
 * rearming the notification) may themselves enter an alertable wait. */
static void CALLBACK status_changed(void *context)
{
    SERVICE_NOTIFYW *notice = context;
    service_native *native = notice->pContext;
    native->status_ready = true;
    native->status_armed = false;
}

static void CALLBACK manager_changed(void *context)
{
    SERVICE_NOTIFYW *notice = context;
    service_native *native = notice->pContext;
    native->manager_ready = true;
    native->manager_armed = false;
}

static bool arm_status(service_native *native)
{
    if (native->status_armed)
        return true;
    memset(&native->status_notice, 0, sizeof(native->status_notice));
    native->status_notice.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    native->status_notice.pfnNotifyCallback = status_changed;
    native->status_notice.pContext = native;
    DWORD error = NotifyServiceStatusChangeW(native->service, STATE_NOTIFICATIONS,
                                            &native->status_notice);
    if (error != ERROR_SUCCESS) {
        publish_error(native, error);
        return false;
    }
    native->status_armed = true;
    return true;
}

static void arm_manager(service_native *native)
{
    if (!native->manager || native->manager_armed)
        return;
    if (native->manager_notice.pszServiceNames)
        LocalFree(native->manager_notice.pszServiceNames);
    memset(&native->manager_notice, 0, sizeof(native->manager_notice));
    native->manager_notice.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    native->manager_notice.pfnNotifyCallback = manager_changed;
    native->manager_notice.pContext = native;
    DWORD error = NotifyServiceStatusChangeW(native->manager,
        SERVICE_NOTIFY_CREATED | SERVICE_NOTIFY_DELETED, &native->manager_notice);
    if (error == ERROR_SUCCESS)
        native->manager_armed = true;
    else
        publish_error(native, error);
}

static void read_status(service_native *native)
{
    if (!native->service) {
        native->service = OpenServiceW(native->manager, PRODUCT_SERVICE,
                                       SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        if (!native->service) {
            publish_error(native, GetLastError());
            return;
        }
    }
    if (!arm_status(native))
        return;
    SERVICE_STATUS_PROCESS status;
    DWORD needed = 0;
    if (!QueryServiceStatusEx(native->service, SC_STATUS_PROCESS_INFO,
                              (BYTE *)&status, sizeof(status), &needed)) {
        publish_error(native, GetLastError());
        return;
    }
    QueryServiceConfigW(native->service, NULL, 0, &needed);
    QUERY_SERVICE_CONFIGW *config = malloc(needed);
    if (!config) {
        publish_error(native, ERROR_NOT_ENOUGH_MEMORY);
        return;
    }
    if (!QueryServiceConfigW(native->service, config, needed, &needed)) {
        DWORD error = GetLastError();
        free(config);
        publish_error(native, error);
        return;
    }
    service_snapshot snapshot = {XP_SERVICE_UNKNOWN, XP_SERVICE_OK,
        config->dwStartType != SERVICE_DISABLED,
        (status.dwControlsAccepted & SERVICE_ACCEPT_STOP) != 0, ""};
    free(config);
    switch (status.dwCurrentState) {
    case SERVICE_STOPPED:
        /* SCM reports 1077 before the first start; it is not a service failure. */
        snapshot.state = status.dwWin32ExitCode == NO_ERROR
            || status.dwWin32ExitCode == ERROR_SERVICE_NEVER_STARTED
            ? XP_SERVICE_STOPPED : XP_SERVICE_FAILED;
        break;
    case SERVICE_RUNNING: snapshot.state = XP_SERVICE_RUNNING; break;
    case SERVICE_START_PENDING:
    case SERVICE_CONTINUE_PENDING: snapshot.state = XP_SERVICE_STARTING; break;
    case SERVICE_STOP_PENDING: snapshot.state = XP_SERVICE_STOPPING; break;
    default: break;
    }
    snprintf(snapshot.detail, sizeof(snapshot.detail),
        "SCM state %lu; exit %lu; service exit %lu%s",
        (unsigned long)status.dwCurrentState, (unsigned long)status.dwWin32ExitCode,
        (unsigned long)status.dwServiceSpecificExitCode,
        snapshot.start_allowed ? "" : "; manual start disabled");
    publish(native, &snapshot);
}

static void detach_monitors(service_native *native)
{
    if (native->service)
        CloseServiceHandle(native->service);
    if (native->manager)
        CloseServiceHandle(native->manager);
    native->service = native->manager = NULL;
    /* Closing prevents new APCs; drain already queued APCs while the buffers
     * and callback context still exist. This does not wait for service exit. */
    SleepEx(0, TRUE);
    native->status_armed = native->manager_armed = false;
    native->status_ready = native->manager_ready = false;
    if (native->manager_notice.pszServiceNames) {
        LocalFree(native->manager_notice.pszServiceNames);
        native->manager_notice.pszServiceNames = NULL;
    }
}

static void disconnect_service(void *context)
{
    service_native *native = context;
    native->attached = false;
    detach_monitors(native);
    native->retry_at = native->retry_delay = 0;
    if (native->helper) {
        /* Closing our handle does not terminate an accepted helper operation. */
        CloseHandle(native->helper);
        native->helper = NULL;
    }
}

static void attach_monitors(service_native *native)
{
    native->manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!native->manager) { publish_error(native, GetLastError()); return; }
    arm_manager(native);
    read_status(native);
    if (!native->retry_at) native->retry_delay = 0;
}

static void connect_service(void *context, uint64_t generation, service_receiver receiver)
{
    service_native *native = context;
    disconnect_service(native);
    native->receiver = receiver;
    native->generation = generation;
    native->revision = 0;
    native->attached = true;
    attach_monitors(native);
}

static void request_service(void *context, uint64_t operation, service_action action)
{
    service_native *native = context;
    DWORD error = ERROR_SUCCESS;
    if (native->authorize) {
        wchar_t *path = NULL;
        error = service_helper_path_win32(&path);
        if (!error) {
            size_t length = wcslen(path) + 32;
            wchar_t *command = calloc(length, sizeof(wchar_t));
            if (!command) error = ERROR_NOT_ENOUGH_MEMORY;
            else {
                swprintf(command, length, L"\"%ls\" %ls", path,
                    action == XP_SERVICE_START ? L"--start" : L"--stop");
                STARTUPINFOW startup = {0};
                PROCESS_INFORMATION process;
                startup.cb = sizeof(startup);
                if (CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                    NULL, NULL, &startup, &process)) {
                    native->helper = process.hProcess;
                    native->operation = operation;
                    CloseHandle(process.hThread);
                } else error = GetLastError();
                free(command);
            }
            free(path);
        }
        if (!error) return;
        char detail[256];
        describe_error(error, detail, sizeof(detail));
        native->receiver.result(native->receiver.context, native->generation, operation,
                                classify_error(error), detail);
        return;
    }
    SC_HANDLE service = OpenServiceW(native->manager, PRODUCT_SERVICE,
        action == XP_SERVICE_START ? SERVICE_START : SERVICE_STOP);
    if (!service) {
        error = GetLastError();
    } else {
        SERVICE_STATUS status;
        BOOL accepted = action == XP_SERVICE_START
            ? StartServiceW(service, 0, NULL)
            : ControlService(service, SERVICE_CONTROL_STOP, &status);
        if (!accepted)
            error = GetLastError();
        CloseServiceHandle(service);
    }
    char detail[256] = "Service request accepted";
    if (error != ERROR_SUCCESS)
        describe_error(error, detail, sizeof(detail));
    native->receiver.result(native->receiver.context, native->generation, operation,
        error == ERROR_SUCCESS ? XP_SERVICE_OK : classify_error(error), detail);
    read_status(native);
}

service_native *service_native_create(void)
{
    return calloc(1, sizeof(service_native));
}

void service_native_enable_authorization(service_native *native)
{
    native->authorize = true;
}

service_control service_native_control(service_native *native)
{
    service_control control = {native, connect_service, request_service, disconnect_service};
    return control;
}

bool service_native_set_wait_handles(service_native *native, void *const *handles, unsigned count)
{
    if (count > 4) return false;
    for (unsigned i = 0; i < count; i++) native->extra_wait[i] = handles[i];
    native->extra_count = count;
    return true;
}

void service_native_set_dialog(service_native *native, void *window) { native->dialog = window; }

void service_native_dispatch(service_native *native, unsigned timeout_ms)
{
    if (native->retry_at) {
        ULONGLONG now = GetTickCount64();
        unsigned remaining = native->retry_at > now ? (unsigned)(native->retry_at - now) : 0;
        if (remaining < timeout_ms) timeout_ms = remaining;
    }
    HANDLE handles[5];
    DWORD count = 0;
    if (native->helper) handles[count++] = native->helper;
    for (unsigned i = 0; i < native->extra_count; i++) handles[count++] = native->extra_wait[i];
    if (!native->status_ready && !native->manager_ready)
        MsgWaitForMultipleObjectsEx(count, handles, timeout_ms, QS_ALLINPUT,
                                    MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
    if (native->retry_at && GetTickCount64() >= native->retry_at) {
        native->retry_at = 0;
        detach_monitors(native);
        attach_monitors(native);
    }
    if (native->helper && WaitForSingleObject(native->helper, 0) == WAIT_OBJECT_0) {
        DWORD exit_code = ERROR_SUCCESS;
        if (!GetExitCodeProcess(native->helper, &exit_code)) exit_code = GetLastError();
        CloseHandle(native->helper);
        native->helper = NULL;
        char detail[256] = "Service request accepted";
        if (exit_code) describe_error(exit_code, detail, sizeof(detail));
        native->receiver.result(native->receiver.context, native->generation, native->operation,
            exit_code ? classify_error(exit_code) : XP_SERVICE_OK, detail);
        read_status(native);
    }
    if (native->manager_ready) {
        native->manager_ready = false;
        if (native->manager_notice.dwNotificationStatus != ERROR_SUCCESS)
            publish_error(native, native->manager_notice.dwNotificationStatus);
        else {
            arm_manager(native);
            if (!native->service) read_status(native);
        }
    }
    if (native->status_ready) {
        native->status_ready = false;
        DWORD error = native->status_notice.dwNotificationStatus;
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE
            || (native->status_notice.dwNotificationTriggered & SERVICE_NOTIFY_DELETE_PENDING)) {
            CloseServiceHandle(native->service);
            native->service = NULL;
            publish_error(native, ERROR_SERVICE_MARKED_FOR_DELETE);
        } else if (error != ERROR_SUCCESS) {
            publish_error(native, error);
        } else {
            read_status(native);
        }
    }
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        if (native->dialog && IsWindowVisible(native->dialog) && IsDialogMessageW(native->dialog, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void service_native_destroy(service_native *native)
{
    if (!native)
        return;
    disconnect_service(native);
    free(native);
}

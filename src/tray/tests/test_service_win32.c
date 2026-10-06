#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <assert.h>
#include <string.h>

static SERVICE_STATUS_PROCESS scm_status;

static BOOL WINAPI query_status(SC_HANDLE service, SC_STATUS_TYPE type,
    BYTE *buffer, DWORD size, DWORD *needed)
{
    (void)service;
    assert(type == SC_STATUS_PROCESS_INFO);
    assert(size >= sizeof(scm_status));
    *needed = sizeof(scm_status);
    memcpy(buffer, &scm_status, sizeof(scm_status));
    return TRUE;
}

static BOOL WINAPI query_config(SC_HANDLE service, QUERY_SERVICE_CONFIGW *config,
    DWORD size, DWORD *needed)
{
    (void)service;
    *needed = sizeof(*config);
    if (size < *needed) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    memset(config, 0, sizeof(*config));
    config->dwStartType = SERVICE_DEMAND_START;
    return TRUE;
}

/* Supply SCM replies without changing the machine's installed service. The
 * production reader must publish the corresponding user-visible state. */
#define QueryServiceStatusEx query_status
#define QueryServiceConfigW query_config
#include "../service_win32.c"
#undef QueryServiceStatusEx
#undef QueryServiceConfigW

static void observe(void *context, uint64_t generation, uint64_t revision,
    const service_snapshot *snapshot)
{
    (void)generation;
    (void)revision;
    *(service_snapshot *)context = *snapshot;
}

int main(void)
{
    service_snapshot snapshot = {0};
    service_native native = {0};
    native.attached = native.status_armed = true;
    native.service = (SC_HANDLE)(uintptr_t)1;
    native.receiver.context = &snapshot;
    native.receiver.observe = observe;
    const struct {
        DWORD state, exit_code;
        service_state expected;
    } cases[] = {
        {SERVICE_STOPPED, ERROR_SERVICE_NEVER_STARTED, XP_SERVICE_STOPPED},
        {SERVICE_STOPPED, NO_ERROR, XP_SERVICE_STOPPED},
        {SERVICE_STOPPED, ERROR_PROCESS_ABORTED, XP_SERVICE_FAILED},
        {SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, XP_SERVICE_FAILED},
        {SERVICE_RUNNING, NO_ERROR, XP_SERVICE_RUNNING},
        {SERVICE_START_PENDING, NO_ERROR, XP_SERVICE_STARTING},
        {SERVICE_STOP_PENDING, NO_ERROR, XP_SERVICE_STOPPING}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        scm_status.dwCurrentState = cases[i].state;
        scm_status.dwWin32ExitCode = cases[i].exit_code;
        scm_status.dwControlsAccepted = cases[i].state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
        read_status(&native);
        assert(snapshot.state == cases[i].expected);
        assert(snapshot.error == XP_SERVICE_OK);
        assert(snapshot.start_allowed);
        assert(snapshot.stop_allowed == (cases[i].state == SERVICE_RUNNING));
    }
    return 0;
}

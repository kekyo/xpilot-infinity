/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * Windows Service Control Manager integration for the dedicated server.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _WINDOWS

# include "xpserver.h"
# include "windows_service.h"

# include <fcntl.h>

#define WINDOWS_SERVICE_NAME L"XPilotInfinityServer"
#define WINDOWS_SERVICE_ARGUMENT "--windows-service"
#define WINDOWS_SERVICE_LOG_ARGUMENT "--windows-service-log"
#define WINDOWS_SERVICE_WAIT_HINT 30000
#define WINDOWS_STDOUT_DESCRIPTOR 1
#define WINDOWS_STDERR_DESCRIPTOR 2

static SERVICE_STATUS_HANDLE service_status_handle;
static volatile LONG service_state = SERVICE_STOPPED;
static DWORD service_specific_exit_code = 1;
static windows_service_run_fn service_run_server;
static int service_argument_count;
static char **service_arguments;
static const char *service_log_path;

static LONG Windows_service_read_state(void)
{
    return InterlockedCompareExchange(&service_state, SERVICE_STOPPED,
				      SERVICE_STOPPED);
}

static void Windows_service_set_status(DWORD state, DWORD win32_exit_code,
				       DWORD service_exit_code,
				       DWORD checkpoint, DWORD wait_hint)
{
    SERVICE_STATUS status;

    if (service_status_handle == NULL)
	return;

    memset(&status, 0, sizeof(status));
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    if (state == SERVICE_RUNNING)
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP
	    | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode = win32_exit_code;
    status.dwServiceSpecificExitCode = service_exit_code;
    status.dwCheckPoint = checkpoint;
    status.dwWaitHint = wait_hint;
    SetServiceStatus(service_status_handle, &status);
}

static void Windows_service_report_current_status(void)
{
    DWORD state = (DWORD)Windows_service_read_state();

    Windows_service_set_status(state, NO_ERROR, 0, 0,
	state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING
	    ? WINDOWS_SERVICE_WAIT_HINT : 0);
}

static void Windows_service_report_stopped(void)
{
    DWORD win32_exit_code;
    DWORD specific_exit_code;

    if (InterlockedExchange(&service_state, SERVICE_STOPPED)
	== SERVICE_STOPPED)
	return;

    if (service_specific_exit_code == 0) {
	win32_exit_code = NO_ERROR;
	specific_exit_code = 0;
    } else {
	win32_exit_code = ERROR_SERVICE_SPECIFIC_ERROR;
	specific_exit_code = service_specific_exit_code;
    }
    Windows_service_set_status(SERVICE_STOPPED, win32_exit_code,
			       specific_exit_code, 0, 0);
}

static DWORD WINAPI Windows_service_control_handler(DWORD control,
					     DWORD event_type,
					     void *event_data,
					     void *context)
{
    LONG current_state;

    UNUSED_PARAM(event_type);
    UNUSED_PARAM(event_data);
    UNUSED_PARAM(context);

    if (control == SERVICE_CONTROL_INTERROGATE) {
	Windows_service_report_current_status();
	return NO_ERROR;
    }
    if (control != SERVICE_CONTROL_STOP
	&& control != SERVICE_CONTROL_SHUTDOWN)
	return ERROR_CALL_NOT_IMPLEMENTED;

    do {
	current_state = Windows_service_read_state();
	if (current_state != SERVICE_START_PENDING
	    && current_state != SERVICE_RUNNING
	    && current_state != SERVICE_STOP_PENDING)
	    return NO_ERROR;
    } while (current_state != SERVICE_STOP_PENDING
	&& InterlockedCompareExchange(&service_state, SERVICE_STOP_PENDING,
				      current_state) != current_state);

    Windows_service_set_status(SERVICE_STOP_PENDING, NO_ERROR, 0, 1,
			       WINDOWS_SERVICE_WAIT_HINT);
    stop_sched();
    return NO_ERROR;
}

static bool Windows_service_redirect_descriptor(const char *path,
					 DWORD standard_handle,
					 int descriptor)
{
    HANDLE file_handle;
    intptr_t duplicated_handle;
    int file_descriptor;

    wchar_t *wide_path = Xp_wide(path);
    if (!wide_path)
        return false;
    file_handle = CreateFileW(wide_path, FILE_APPEND_DATA,
			      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide_path);
    if (file_handle == INVALID_HANDLE_VALUE)
	return false;

    file_descriptor = _open_osfhandle((intptr_t)file_handle,
				      _O_APPEND | _O_TEXT);
    if (file_descriptor < 0) {
	CloseHandle(file_handle);
	return false;
    }
    if (_dup2(file_descriptor, descriptor) < 0) {
	_close(file_descriptor);
	return false;
    }
    _close(file_descriptor);

    duplicated_handle = _get_osfhandle(descriptor);
    if (duplicated_handle == -1
	|| !SetStdHandle(standard_handle, (HANDLE)duplicated_handle))
	return false;
    return true;
}

static bool Windows_service_redirect_output(void)
{
    if (service_log_path == NULL)
	return true;

    if (!Windows_service_redirect_descriptor(service_log_path,
					     STD_OUTPUT_HANDLE,
					     WINDOWS_STDOUT_DESCRIPTOR))
	return false;
    if (!Windows_service_redirect_descriptor(service_log_path,
					     STD_ERROR_HANDLE,
					     WINDOWS_STDERR_DESCRIPTOR))
	return false;
    return true;
}

static void WINAPI Windows_service_main(DWORD argc, wchar_t **argv)
{
    int exit_code;

    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    service_status_handle = RegisterServiceCtrlHandlerExW(
	WINDOWS_SERVICE_NAME, Windows_service_control_handler, NULL);
    if (service_status_handle == NULL)
	return;

    InterlockedExchange(&service_state, SERVICE_START_PENDING);
    Windows_service_set_status(SERVICE_START_PENDING, NO_ERROR, 0, 1,
			       WINDOWS_SERVICE_WAIT_HINT);
    if (atexit(Windows_service_report_stopped) != 0) {
	service_specific_exit_code = ERROR_NOT_ENOUGH_MEMORY;
	Windows_service_report_stopped();
	return;
    }
    if (!Windows_service_redirect_output()) {
	service_specific_exit_code = ERROR_OPEN_FAILED;
	Windows_service_report_stopped();
	return;
    }

    exit_code = service_run_server(service_argument_count,
				   service_arguments);
    service_specific_exit_code = exit_code == 0 ? 0 : (DWORD)exit_code;
    Windows_service_report_stopped();
}

static bool Windows_service_prepare_arguments(int argc, char **argv)
{
    int source_index;
    int destination_index = 0;

    service_arguments = malloc(((size_t)argc + 1) * sizeof(*service_arguments));
    if (service_arguments == NULL)
	return false;

    for (source_index = 0; source_index < argc; source_index++) {
	if (strcmp(argv[source_index], WINDOWS_SERVICE_ARGUMENT) == 0)
	    continue;
	if (strcmp(argv[source_index], WINDOWS_SERVICE_LOG_ARGUMENT) == 0) {
	    if (source_index + 1 >= argc) {
		free(service_arguments);
		service_arguments = NULL;
		return false;
	    }
	    service_log_path = argv[++source_index];
	    continue;
	}
	service_arguments[destination_index++] = argv[source_index];
    }
    service_arguments[destination_index] = NULL;
    service_argument_count = destination_index;
    return true;
}

bool Windows_service_requested(int argc, char **argv)
{
    int index;

    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], WINDOWS_SERVICE_ARGUMENT) == 0)
	    return true;
    }
    return false;
}

int Windows_service_dispatch(int argc, char **argv,
			     windows_service_run_fn run_server)
{
    SERVICE_TABLE_ENTRYW dispatch_table[] = {
	{ WINDOWS_SERVICE_NAME, Windows_service_main },
	{ NULL, NULL }
    };
    DWORD error_code;
    int exit_code;

    if (run_server == NULL || !Windows_service_prepare_arguments(argc, argv)) {
	fputs("Invalid Windows service arguments.\n", stderr);
	return 1;
    }
    service_run_server = run_server;

    if (!StartServiceCtrlDispatcherW(dispatch_table)) {
	error_code = GetLastError();
	fprintf(stderr, "Windows service dispatcher failed (%lu).\n",
		(unsigned long)error_code);
	free(service_arguments);
	service_arguments = NULL;
	return 1;
    }

    exit_code = service_specific_exit_code == 0 ? 0 : 1;
    free(service_arguments);
    service_arguments = NULL;
    return exit_code;
}

void Windows_service_report_running(void)
{
    if (InterlockedCompareExchange(&service_state, SERVICE_RUNNING,
				   SERVICE_START_PENDING)
	!= SERVICE_START_PENDING)
	return;
    Windows_service_set_status(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
}

void Windows_service_prepare_clean_exit(void)
{
    LONG current_state = Windows_service_read_state();

    if (current_state == SERVICE_RUNNING
	|| current_state == SERVICE_STOP_PENDING)
	service_specific_exit_code = 0;
}

#endif /* _WINDOWS */

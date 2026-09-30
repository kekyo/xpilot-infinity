#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <sddl.h>
#include <share.h>
#include "tray_platform.h"
#include "tray_menu.h"
#include "service_native.h"
#include "service_config.h"
#include "service_paths_win32.h"
#include "settings_file_win32.h"
#include "utf8_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRAY_EVENT (WM_APP + 1)
#define WINDOW_CLASS L"XPilotInfinityServerTray"

typedef struct {
    HWND window;
    HWND rows[8];
    HICON icon;
    HANDLE mutex;
    NOTIFYICONDATAW notification;
    service_native *native;
    tray_controller *controller;
    tray_menu menu;
    bool visible_icon;
    bool done;
    char *configured_map;
    char generation[65];
    char settings_detail[512];
    char last_saved_detail[512];
    map_catalog maps;
    char *map_directory;
    HANDLE settings_process;
} windows_tray;

static wchar_t *wide(const char *text)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!length) return NULL;
    wchar_t *result = malloc((size_t)length * sizeof(wchar_t));
    if (result) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, length);
    return result;
}

static void read_map(windows_tray *tray)
{
    free(tray->configured_map);
    free(tray->map_directory);
    tray->configured_map = tray->map_directory = NULL;
    tray->generation[0] = 0;
    tray->last_saved_detail[0] = 0;
    map_catalog_clear(&tray->maps);
    wchar_t *directory = NULL, *path = NULL;
    DWORD error = service_paths_win32(&directory, &path);
    if (error) {
        snprintf(tray->settings_detail, sizeof(tray->settings_detail),
            "Map changes unavailable: the service registration is absent, inaccessible, or uses unsupported overrides (error %lu).",
            (unsigned long)error);
        return;
    }
    char *text = NULL;
    error = settings_win32_read(path, &text, tray->generation);
    if (!error) {
        tray->configured_map = service_config_map(text, false);
        char *check = service_config_select_map(text, "C:\\validation.xp2", false);
        if (!check) {
            tray->generation[0] = 0;
            snprintf(tray->settings_detail, sizeof(tray->settings_detail),
                "Map changes unavailable: configuration has unsupported map overrides or syntax.");
        }
        free(check);
    } else snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Cannot read the shared UTF-8 configuration (error %lu).", (unsigned long)error);
    free(text);
    size_t result_length = wcslen(path) + 8;
    wchar_t *result_path = calloc(result_length, sizeof(wchar_t));
    if (result_path) {
        swprintf(result_path, result_length, L"%ls.result", path);
        char *record = NULL, record_generation[65];
        if (!settings_win32_read(result_path, &record, record_generation) && strlen(record) < 2048) {
            char *line = strchr(record, '\n');
            if (line) {
                *line++ = 0;
                if (!strcmp(record, tray->generation))
                    snprintf(tray->last_saved_detail, sizeof(tray->last_saved_detail), "%s", line);
            }
        }
        free(record); free(result_path);
    }
    size_t length = wcslen(directory) + 16;
    wchar_t *maps = calloc(length, sizeof(wchar_t));
    if (maps) {
        swprintf(maps, length, L"%ls\\lib\\maps", directory);
        tray->map_directory = Xp_utf8(maps);
        if (tray->map_directory) map_catalog_read(tray->map_directory, &tray->maps);
        free(maps);
    }
    free(directory); free(path);
}

static void start_map(windows_tray *tray, const char *name)
{
    wchar_t *helper = NULL;
    DWORD error = service_helper_path_win32(&helper);
    size_t bytes = strlen(name);
    wchar_t *hex = calloc(bytes * 2 + 1, sizeof(wchar_t));
    wchar_t *generation = wide(tray->generation);
    if (!hex || !generation) error = ERROR_NOT_ENOUGH_MEMORY;
    if (!error) {
        for (size_t i = 0; i < bytes; i++) {
            hex[i * 2] = L"0123456789abcdef"[(unsigned char)name[i] >> 4];
            hex[i * 2 + 1] = L"0123456789abcdef"[(unsigned char)name[i] & 15];
        }
        size_t length = wcslen(helper) + wcslen(generation) + bytes * 2 + 32;
        wchar_t *command = calloc(length, sizeof(wchar_t));
        if (!command) error = ERROR_NOT_ENOUGH_MEMORY;
        else {
            swprintf(command, length, L"\"%ls\" --map %ls %ls", helper, generation, hex);
            STARTUPINFOW startup = {0};
            PROCESS_INFORMATION process;
            startup.cb = sizeof(startup);
            if (!CreateProcessW(helper, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                NULL, NULL, &startup, &process)) error = GetLastError();
            else {
                CloseHandle(process.hThread);
                tray->settings_process = process.hProcess;
                service_native_set_wait_handle(tray->native, process.hProcess);
                snprintf(tray->settings_detail, sizeof(tray->settings_detail),
                    "Authorizing and saving the selected map. A running service will restart.");
            }
            free(command);
        }
    }
    if (error) snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Could not start the settings helper (error %lu).", (unsigned long)error);
    free(helper); free(hex); free(generation);
}

static void finish_map(windows_tray *tray)
{
    if (!tray->settings_process || WaitForSingleObject(tray->settings_process, 0) != WAIT_OBJECT_0) return;
    DWORD error = ERROR_SUCCESS;
    if (!GetExitCodeProcess(tray->settings_process, &error)) error = GetLastError();
    service_native_set_wait_handle(tray->native, NULL);
    CloseHandle(tray->settings_process);
    tray->settings_process = NULL;
    read_map(tray);
    if (!error) snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Configuration saved. A running service was restarted; game readiness and the active map remain unverified.");
    else if (error & 0x20000000UL) snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Configuration saved, but service application failed (error %lu). The saved map is not confirmed active.",
        (unsigned long)(error & ~0x20000000UL));
    else if (error == ERROR_CANCELLED) snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Authorization canceled. Configuration was not changed.");
    else if (error == ERROR_REVISION_MISMATCH) snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "The shared configuration or service changed during this operation. Refresh and try again.");
    else snprintf(tray->settings_detail, sizeof(tray->settings_detail),
        "Configuration was not changed (error %lu). Check permissions and the service configuration.", (unsigned long)error);
    if (error) {
        wchar_t *message = wide(tray->settings_detail);
        if (message) MessageBoxW(tray->window, message, L"XPilot Infinity Server", MB_OK | MB_ICONWARNING);
        free(message);
    }
}

static void update(windows_tray *tray)
{
    unsigned revision = tray->menu.revision;
    tray_status status = *tray_controller_status(tray->controller);
    if (tray->settings_process) { status.busy = true; status.can_start = status.can_stop = false; }
    tray_menu_update(&tray->menu, &status,
        tray->configured_map ? tray->configured_map : "Unavailable or managed by other service settings");
    const char *selected = NULL;
    if (tray->configured_map && tray->map_directory) {
        const char *name = strrchr(tray->configured_map, '\\');
        if (!name && !strchr(tray->configured_map, '/')) selected = tray->configured_map;
        else if (name && (size_t)(name - tray->configured_map) == strlen(tray->map_directory)
            && !memcmp(tray->configured_map, tray->map_directory, strlen(tray->map_directory))) selected = name + 1;
    }
    bool stable = status.service.state == XP_SERVICE_RUNNING || status.service.state == XP_SERVICE_STOPPED
        || status.service.state == XP_SERVICE_FAILED;
    tray_menu_set_maps(&tray->menu, &tray->maps, selected, stable && !status.busy && tray->generation[0],
        status.service.state == XP_SERVICE_RUNNING);
    if (revision == tray->menu.revision) return;
    for (size_t i = 0; i < sizeof(tray->menu.items) / sizeof(tray->menu.items[0]); i++) {
        wchar_t *label = wide(tray->menu.items[i].label);
        if (label) {
            SetWindowTextW(tray->rows[i], label);
            free(label);
        }
        if (tray->menu.items[i].id >= TRAY_START)
            EnableWindow(tray->rows[i], tray->menu.items[i].enabled);
    }
    wchar_t *title = wide(tray->menu.items[0].label);
    if (title) {
        wcsncpy(tray->notification.szTip, title, 127);
        tray->notification.szTip[127] = 0;
        free(title);
        if (tray->visible_icon) Shell_NotifyIconW(NIM_MODIFY, &tray->notification);
    }
}

static void popup(windows_tray *tray, POINT point);

static void activate(windows_tray *tray, int id)
{
    const tray_menu_item *item = tray_menu_find(&tray->menu, id);
    if (!item || !item->enabled) return;
    if (id == TRAY_START || id == TRAY_STOP)
        tray_controller_request(tray->controller, id == TRAY_START ? XP_SERVICE_START : XP_SERVICE_STOP);
    else if (id == TRAY_QUIT)
        tray->done = true;
    else if (id == TRAY_MAP_MENU) {
        POINT point; GetCursorPos(&point); popup(tray, point);
    } else if (item->radio && !item->checked) start_map(tray, item->label);
    else if (id == TRAY_DETAILS) {
        const tray_status *status = tray_controller_status(tray->controller);
        char detail[2048];
        snprintf(detail, sizeof(detail), "%s\n%s\n%s\n%s\n\nService: XPilotInfinityServer\n"
            "Configuration / logs: %%ProgramData%%\\XPilot Infinity\\server\n\n"
            "Install the optional Dedicated server service component if the service is absent.\n"
            "Start and stop may require administrator authentication.\n"
            "Quitting this tray leaves the server running.",
            status->service.detail, status->operation_detail, tray->settings_detail, tray->last_saved_detail);
        wchar_t *message = wide(detail);
        if (message) MessageBoxW(tray->window, message, L"XPilot Infinity Server", MB_OK | MB_ICONINFORMATION);
        free(message);
    }
    update(tray);
}

static void append_row(HMENU menu, const tray_menu_item *item, HMENU submenu)
{
    wchar_t *label = wide(item->label);
    if (!label) return;
    size_t length = wcslen(label);
    wchar_t *escaped = calloc(length * 2 + 1, sizeof(wchar_t));
    if (escaped) {
        size_t written = 0;
        for (size_t j = 0; j < length; j++) {
            escaped[written++] = label[j];
            if (label[j] == L'&') escaped[written++] = L'&';
        }
        MENUITEMINFOW row = {0};
        row.cbSize = sizeof(row);
        row.fMask = MIIM_STRING | MIIM_STATE | MIIM_ID | MIIM_FTYPE;
        row.wID = (UINT)item->id;
        row.fType = item->radio ? MFT_RADIOCHECK : MFT_STRING;
        row.fState = (item->enabled ? MFS_ENABLED : MFS_DISABLED) | (item->checked ? MFS_CHECKED : MFS_UNCHECKED);
        row.dwTypeData = escaped;
        if (submenu) { row.fMask |= MIIM_SUBMENU; row.hSubMenu = submenu; }
        InsertMenuItemW(menu, (UINT)-1, TRUE, &row);
        free(escaped);
    }
    free(label);
}

static void popup(windows_tray *tray, POINT point)
{
    read_map(tray);
    update(tray);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (size_t i = 0; i < sizeof(tray->menu.items) / sizeof(tray->menu.items[0]); i++) {
        const tray_menu_item *item = &tray->menu.items[i];
        if (item->id == TRAY_SEPARATOR) AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        else if (item->id == TRAY_MAP_MENU) {
            HMENU maps = CreatePopupMenu();
            if (!maps) continue;
            for (size_t j = 0; j < tray->menu.map_count; j++) append_row(maps, &tray->menu.maps[j], 0);
            append_row(menu, item, maps);
        } else append_row(menu, item, 0);
    }
    if (point.x == -1 && point.y == -1) GetCursorPos(&point);
    SetForegroundWindow(tray->window);
    int id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                            point.x, point.y, 0, tray->window, NULL);
    PostMessageW(tray->window, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (id) activate(tray, id);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    windows_tray *tray = (windows_tray *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *creation = (CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)creation->lpCreateParams);
        return TRUE;
    }
    if (!tray) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_COMMAND:
        activate(tray, LOWORD(wparam));
        return 0;
    case TRAY_EVENT:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == NIN_SELECT
            || LOWORD(lparam) == NIN_KEYSELECT) {
            POINT point = {GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)};
            popup(tray, point);
        }
        return 0;
    case WM_CLOSE:
        if (tray->visible_icon) ShowWindow(window, SW_HIDE);
        else tray->done = true;
        return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION:
        if (wparam) tray->done = true;
        return 0;
    default: return DefWindowProcW(window, message, wparam, lparam);
    }
}

static HANDLE instance_mutex(void)
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return NULL;
    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &needed);
    TOKEN_USER *user = malloc(needed);
    wchar_t *sid = NULL;
    if (!user || !GetTokenInformation(token, TokenUser, user, needed, &needed)
        || !ConvertSidToStringSidW(user->User.Sid, &sid)) {
        free(user); CloseHandle(token); return NULL;
    }
    wchar_t name[256];
    swprintf(name, 256, L"Local\\XPilotInfinityServerTray-%ls", sid);
    LocalFree(sid); free(user); CloseHandle(token);
    HANDLE mutex = CreateMutexW(NULL, FALSE, name);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(WINDOW_CLASS, NULL);
        if (existing) { ShowWindow(existing, SW_SHOW); SetForegroundWindow(existing); }
        CloseHandle(mutex);
        return NULL;
    }
    return mutex;
}

int tray_platform_run(int argc, char **argv)
{
    (void)argc; (void)argv;
    windows_tray tray = {0};
    tray.mutex = instance_mutex();
    if (!tray.mutex) return 0;
    tray.native = service_native_create();
    if (!tray.native) { CloseHandle(tray.mutex); return 1; }
    service_native_enable_authorization(tray.native);
    tray.controller = tray_controller_create(service_native_control(tray.native));
    if (!tray.controller) { service_native_destroy(tray.native); CloseHandle(tray.mutex); return 1; }
    tray_controller_connect(tray.controller);
    read_map(&tray);
    tray_menu_update(&tray.menu, tray_controller_status(tray.controller),
                      tray.configured_map ? tray.configured_map : "Unavailable");
    HINSTANCE instance = GetModuleHandleW(NULL);
    tray.icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    WNDCLASSW window_class = {0};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = WINDOW_CLASS;
    window_class.hIcon = tray.icon;
    window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&window_class);
    tray.window = CreateWindowExW(0, WINDOW_CLASS, L"XPilot Infinity Server",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 370,
        NULL, NULL, instance, &tray);
    if (!tray.window) tray.done = true;
    for (size_t i = 0; tray.window && i < sizeof(tray.menu.items) / sizeof(tray.menu.items[0]); i++) {
        const tray_menu_item *item = &tray.menu.items[i];
        wchar_t *label = wide(item->label);
        tray.rows[i] = CreateWindowExW(0, item->id >= TRAY_START ? L"BUTTON" : L"STATIC",
            label ? label : L"", WS_CHILD | WS_VISIBLE | (item->id >= TRAY_START ? WS_TABSTOP : SS_NOPREFIX),
            16, 16 + (int)i * 40, 510, 32, tray.window,
            (HMENU)(INT_PTR)item->id, instance, NULL);
        free(label);
        SendMessageW(tray.rows[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        if (item->id >= TRAY_START) EnableWindow(tray.rows[i], item->enabled);
    }
    tray.notification.cbSize = sizeof(tray.notification);
    tray.notification.hWnd = tray.window;
    tray.notification.uID = 1;
    tray.notification.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray.notification.uCallbackMessage = TRAY_EVENT;
    tray.notification.hIcon = tray.icon;
    wcscpy(tray.notification.szTip, L"XPilot Infinity Server");
    tray.visible_icon = tray.window && Shell_NotifyIconW(NIM_ADD, &tray.notification);
    if (tray.visible_icon) {
        tray.notification.uVersion = NOTIFYICON_VERSION_4;
        if (!Shell_NotifyIconW(NIM_SETVERSION, &tray.notification)) {
            Shell_NotifyIconW(NIM_DELETE, &tray.notification);
            tray.visible_icon = false;
        }
    }
    if (!tray.visible_icon) ShowWindow(tray.window, SW_SHOW);
    update(&tray);
    while (!tray.done) {
        service_native_dispatch(tray.native, 60000);
        finish_map(&tray);
        update(&tray);
    }
    if (tray.visible_icon) Shell_NotifyIconW(NIM_DELETE, &tray.notification);
    if (tray.window) DestroyWindow(tray.window);
    if (tray.icon) DestroyIcon(tray.icon);
    tray_controller_destroy(tray.controller);
    service_native_destroy(tray.native);
    if (tray.settings_process) CloseHandle(tray.settings_process);
    free(tray.configured_map); free(tray.map_directory);
    map_catalog_clear(&tray.maps);
    tray_menu_clear(&tray.menu);
    CloseHandle(tray.mutex);
    return 0;
}

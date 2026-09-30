#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <sddl.h>
#include "tray_platform.h"
#include "tray_menu.h"
#include "service_native.h"
#include "service_config.h"
#include "service_paths_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRAY_EVENT (WM_APP + 1)
#define WINDOW_CLASS L"XPilotInfinityServerTray"

typedef struct {
    HWND window;
    HWND rows[7];
    HICON icon;
    HANDLE mutex;
    NOTIFYICONDATAW notification;
    service_native *native;
    tray_controller *controller;
    tray_menu menu;
    bool visible_icon;
    bool done;
    char *configured_map;
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
    tray->configured_map = NULL;
    wchar_t *directory, *path;
    if (service_paths_win32(&directory, &path)) return;
    FILE *file = _wfopen(path, L"rb");
    if (file) {
        if (!fseek(file, 0, SEEK_END)) {
            long size = ftell(file);
            if (size >= 0 && size <= 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
                char *text = calloc((size_t)size + 1, 1);
                if (text && fread(text, 1, (size_t)size, file) == (size_t)size)
                    tray->configured_map = service_config_map(text, false);
                free(text);
            }
        }
        fclose(file);
    }
    free(directory); free(path);
}

static void update(windows_tray *tray)
{
    unsigned revision = tray->menu.revision;
    tray_menu_update(&tray->menu, tray_controller_status(tray->controller),
        tray->configured_map ? tray->configured_map : "Unavailable or managed by other service settings");
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

static void activate(windows_tray *tray, int id)
{
    const tray_menu_item *item = tray_menu_find(&tray->menu, id);
    if (!item || !item->enabled) return;
    if (id == TRAY_START || id == TRAY_STOP)
        tray_controller_request(tray->controller, id == TRAY_START ? XP_SERVICE_START : XP_SERVICE_STOP);
    else if (id == TRAY_QUIT)
        tray->done = true;
    else if (id == TRAY_DETAILS) {
        const tray_status *status = tray_controller_status(tray->controller);
        char detail[2048];
        snprintf(detail, sizeof(detail), "%s\n%s\n\nService: XPilotInfinityServer\n"
            "Configuration / logs: %%ProgramData%%\\XPilot Infinity\\server\n\n"
            "Install the optional Dedicated server service component if the service is absent.\n"
            "Start and stop may require administrator authentication.\n"
            "Quitting this tray leaves the server running.",
            status->service.detail, status->operation_detail);
        wchar_t *message = wide(detail);
        if (message) MessageBoxW(tray->window, message, L"XPilot Infinity Server", MB_OK | MB_ICONINFORMATION);
        free(message);
    }
    update(tray);
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
        else {
            wchar_t *label = wide(item->label);
            if (!label) continue;
            size_t length = wcslen(label);
            wchar_t *escaped = calloc(length * 2 + 1, sizeof(wchar_t));
            if (escaped) {
                size_t written = 0;
                for (size_t j = 0; j < length; j++) {
                    escaped[written++] = label[j];
                    if (label[j] == L'&') escaped[written++] = L'&';
                }
                AppendMenuW(menu, MF_STRING | (item->enabled ? MF_ENABLED : MF_GRAYED),
                            (UINT_PTR)item->id, escaped);
                free(escaped);
            }
            free(label);
        }
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
    while (!tray.done) {
        service_native_dispatch(tray.native, 60000);
        update(&tray);
    }
    if (tray.visible_icon) Shell_NotifyIconW(NIM_DELETE, &tray.notification);
    if (tray.window) DestroyWindow(tray.window);
    if (tray.icon) DestroyIcon(tray.icon);
    tray_controller_destroy(tray.controller);
    service_native_destroy(tray.native);
    free(tray.configured_map);
    CloseHandle(tray.mutex);
    return 0;
}

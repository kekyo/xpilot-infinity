/* XPilot Infinity -- GPL-2.0-or-later. */
#include "xpcommon.h"
#include "web_front.h"
#include <fcntl.h>
#ifndef _WINDOWS
#include <arpa/inet.h>
#else
#include "utf8_files.h"
#include <io.h>
#endif

#define WEB_PENDING 64
#define WEB_PER_IP 16
#define WEB_HEADER 8192
#define WEB_CHUNK 16384
#define WEB_FILE_LIMIT (64 * 1024 * 1024)
#define WEB_FRAME_BUDGET (256 * 1024)

typedef struct {
    bool active;
    bool responding;
    bool finishing;
    sock_t socket;
    char address[SOCK_HOSTNAME_LENGTH];
    int port;
    server_endpoint_t endpoint;
    time_t opened, progress;
    FILE *file;
    size_t remaining;
    char output[WEB_CHUNK];
    size_t length, offset;
} web_request_t;

struct web_front {
    int root_fd;
#ifdef _WINDOWS
    HANDLE root_handle;
    wchar_t root_path[WEB_HEADER];
#endif
    char *origin;
    char *proxy;
    web_upgrade_fn upgrade;
    size_t cursor;
    web_request_t requests[WEB_PENDING];
};

static bool Normalize_address(const char *text, char *normalized)
{
    unsigned char address[16];
    if (!text) return false;
    if (inet_pton(AF_INET, text, address) == 1)
        return inet_ntop(AF_INET, address, normalized, SOCK_HOSTNAME_LENGTH) != NULL;
    if (inet_pton(AF_INET6, text, address) != 1) return false;
    static const unsigned char mapped[12] = {0,0,0,0,0,0,0,0,0,0,255,255};
    if (!memcmp(address, mapped, sizeof(mapped)))
        return inet_ntop(AF_INET, address + 12, normalized, SOCK_HOSTNAME_LENGTH) != NULL;
    return inet_ntop(AF_INET6, address, normalized, SOCK_HOSTNAME_LENGTH) != NULL;
}

static bool Valid_host(const char *host)
{
    if (!host || !*host || strlen(host) > 255) return false;
    for (const unsigned char *p = (const unsigned char *)host; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9') || strchr(".-:[]", *p))) return false;
    }
    return true;
}

web_front_t *Web_front_create(const char *root, const char *origin,
                             const char *proxy, web_upgrade_fn upgrade)
{
    if (origin && (!((strncmp(origin, "http://", 7) == 0 && Valid_host(origin + 7))
        || (strncmp(origin, "https://", 8) == 0 && Valid_host(origin + 8))))) {
        errno = EINVAL;
        return NULL;
    }
    char normalized_proxy[SOCK_HOSTNAME_LENGTH];
    if (proxy && (!Normalize_address(proxy, normalized_proxy) || !origin)) {
        errno = EINVAL;
        return NULL;
    }
    web_front_t *front = calloc(1, sizeof(*front));
    if (!front) return NULL;
    front->root_fd = -1;
#ifdef _WINDOWS
    front->root_handle = INVALID_HANDLE_VALUE;
#endif
    front->upgrade = upgrade;
    if (origin) front->origin = strdup(origin);
    if (proxy) front->proxy = strdup(normalized_proxy);
    if ((origin && !front->origin) || (proxy && !front->proxy)) {
        Web_front_destroy(front);
        return NULL;
    }
    if (root) {
#ifndef _WINDOWS
        front->root_fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
#else
        wchar_t *wide = Xp_wide(root);
        if (wide) {
            front->root_handle = CreateFileW(wide, FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS, NULL);
            free(wide);
        }
        BY_HANDLE_FILE_INFORMATION info;
        DWORD length = GetFinalPathNameByHandleW(front->root_handle,
            front->root_path, WEB_HEADER - 1, FILE_NAME_NORMALIZED);
        if (length && length < WEB_HEADER - 1
            && GetFileInformationByHandle(front->root_handle, &info)
            && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (front->root_path[length - 1] != L'\\') front->root_path[length++] = L'\\';
            front->root_path[length] = L'\0';
        } else {
            Web_front_destroy(front);
            errno = EINVAL;
            return NULL;
        }
#endif
#ifndef _WINDOWS
        if (front->root_fd < 0) {
            Web_front_destroy(front);
            return NULL;
        }
#endif
    }
    return front;
}

static void Request_close(web_request_t *request)
{
    if (request->file) fclose(request->file);
    sock_close(&request->socket);
    memset(request, 0, sizeof(*request));
}

void Web_front_destroy(web_front_t *front)
{
    if (!front) return;
    for (size_t i = 0; i < WEB_PENDING; i++)
        if (front->requests[i].active) Request_close(&front->requests[i]);
    if (front->root_fd >= 0) close(front->root_fd);
#ifdef _WINDOWS
    if (front->root_handle != INVALID_HANDLE_VALUE) CloseHandle(front->root_handle);
#endif
    free(front->origin);
    free(front->proxy);
    free(front);
}

void Web_front_accept(web_front_t *front, sock_t *socket, const char *address,
                      int port, const server_endpoint_t *endpoint)
{
    web_request_t *slot = NULL;
    int count = 0;
    for (size_t i = 0; i < WEB_PENDING; i++) {
        web_request_t *request = &front->requests[i];
        if (!request->active) slot = request;
        else if (!strcmp(request->address, address)) count++;
    }
    /* The proxy shares the global bound; game admission still uses real IPs. */
    bool trusted = front->proxy && !strcmp(address, front->proxy);
    if (!slot || (count >= WEB_PER_IP && !trusted)) {
        sock_close(socket);
        return;
    }
    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    slot->socket = *socket;
    sock_init(socket);
    strlcpy(slot->address, address, sizeof(slot->address));
    slot->port = port;
    slot->endpoint = *endpoint;
    slot->opened = slot->progress = time(NULL);
}

static void Response(web_request_t *request, int status, const char *reason,
                     const char *mime, size_t size)
{
    request->responding = true;
    request->length = (size_t)snprintf(request->output, sizeof(request->output),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
        "Connection: close\r\nCache-Control: no-cache\r\n"
        "X-Content-Type-Options: nosniff\r\n%s\r\n",
        status, reason, mime, (unsigned long)size,
        status == 405 ? "Allow: GET, HEAD\r\n" : "");
    request->offset = 0;
}

static const char *Mime(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcmp(ext, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(ext, ".js") || !strcmp(ext, ".mjs")) return "text/javascript; charset=utf-8";
    if (!strcmp(ext, ".css")) return "text/css; charset=utf-8";
    if (!strcmp(ext, ".wasm")) return "application/wasm";
    if (!strcmp(ext, ".json")) return "application/json";
    if (!strcmp(ext, ".png")) return "image/png";
    if (!strcmp(ext, ".svg")) return "image/svg+xml";
    if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcmp(ext, ".ttf")) return "font/ttf";
    if (!strcmp(ext, ".woff")) return "font/woff";
    if (!strcmp(ext, ".woff2")) return "font/woff2";
    if (!strcmp(ext, ".wav")) return "audio/wav";
    if (!strcmp(ext, ".ogg")) return "audio/ogg";
    return "application/octet-stream";
}

static int Hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool Decode_path(char *path)
{
    if (*path != '/') return false;
    char *out = path;
    for (const char *p = path; *p && *p != '?'; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '%') {
            if (!p[1] || !p[2] || Hex(p[1]) < 0 || Hex(p[2]) < 0) return false;
            c = (unsigned char)((Hex(p[1]) << 4) | Hex(p[2]));
            p += 2;
            if (c == '/') return false;
        }
        if (c < 33 || c > 126 || strchr("\\%:#", c)) return false;
        *out++ = (char)c;
    }
    *out = '\0';
    return true;
}

static FILE *Open_asset(web_front_t *front, char *path, size_t *size)
{
    /* Reject hidden/dot components and ambiguous Windows names on every host. */
    for (const char *part = path + 1; ; ) {
        const char *slash = strchr(part, '/');
        size_t length = slash ? (size_t)(slash - part) : strlen(part);
        if (!length || length > 255 || *part == '.' || part[length - 1] == '.') return NULL;
        if (!slash) break;
        part = slash + 1;
    }
#ifndef _WINDOWS
    if (front->root_fd < 0) return NULL;
    int parent = dup(front->root_fd);
    if (parent < 0) return NULL;
    char *part = path + 1;
    for (;;) {
        char *slash = strchr(part, '/');
        if (slash) *slash = '\0';
        if (!*part || *part == '.' || strlen(part) > 255) {
            close(parent);
            return NULL;
        }
        /* Resolve each component relative to an owned descriptor; symlinks,
         * FIFOs and directory replacement cannot escape or block the server. */
        int fd = openat(parent, part, O_RDONLY | O_NOFOLLOW | O_CLOEXEC
            | O_NONBLOCK | (slash ? O_DIRECTORY : 0));
        close(parent);
        if (fd < 0) return NULL;
        if (slash) {
            parent = fd;
            part = slash + 1;
            continue;
        }
        struct stat info;
        if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
            close(fd);
            return NULL;
        }
        *size = (size_t)info.st_size;
        if (info.st_size > WEB_FILE_LIMIT) { close(fd); return NULL; }
        FILE *file = fdopen(fd, "rb");
        if (!file) close(fd);
        return file;
    }
#else
    if (front->root_handle == INVALID_HANDLE_VALUE) return NULL;
    wchar_t full[WEB_HEADER], actual[WEB_HEADER];
    size_t root_length = wcslen(front->root_path);
    if (root_length + strlen(path) >= WEB_HEADER) return NULL;
    wcscpy(full, front->root_path);
    wchar_t *out = full + root_length;
    for (const char *p = path + 1; *p; p++) *out++ = *p == '/' ? L'\\' : (unsigned char)*p;
    *out = 0;
    HANDLE handle = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE) return NULL;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD length = GetFinalPathNameByHandleW(handle, actual, WEB_HEADER, FILE_NAME_NORMALIZED);
    /* Validate the opened handle rather than a path checked before opening.
     * Holding root_handle without delete sharing keeps the root anchored. */
    if (!length || length >= WEB_HEADER || GetFileType(handle) != FILE_TYPE_DISK
        || _wcsnicmp(actual, front->root_path, root_length)
        || !GetFileInformationByHandle(handle, &info)
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(handle);
        return NULL;
    }
    *size = info.nFileSizeHigh ? WEB_FILE_LIMIT + 1 : info.nFileSizeLow;
    if (*size > WEB_FILE_LIMIT) { CloseHandle(handle); return NULL; }
    int fd = _open_osfhandle((intptr_t)handle, _O_RDONLY | _O_BINARY);
    if (fd < 0) { CloseHandle(handle); return NULL; }
    FILE *file = _fdopen(fd, "rb");
    if (!file) _close(fd);
    return file;
#endif
}

static int Read_request(web_front_t *front, web_request_t *request)
{
    char header[WEB_HEADER + 1];
    int count = sock_peek(&request->socket, header, WEB_HEADER);
    if (count <= 0) return count < 0 && sock_error_is_temporary(&request->socket) ? 0 : -1;
    header[count] = '\0';
    char *end = strstr(header, "\r\n\r\n");
    if (!end && count < WEB_HEADER) return 0;
    if (!end) {
        /* Drain the inspected prefix before closing so clients receive 431. */
        sock_read(&request->socket, header, count);
        Response(request, 431, "Request Header Fields Too Large", "text/plain", 0);
        return 0;
    }
    size_t length = (size_t)(end - header) + 4;
    if (memchr(header, '\0', length)) return -1;
    char *line_end = strstr(header, "\r\n");
    *line_end = '\0';
    char *path = strchr(header, ' ');
    char *version = path ? strchr(path + 1, ' ') : NULL;
    if (!path || !version) return -1;
    *path++ = '\0';
    *version++ = '\0';
    bool head = !strcmp(header, "HEAD");
    int status = 200;
    const char *reason = "OK";
    if (strcmp(version, "HTTP/1.1")) { status = 400; reason = "Bad Request"; }
    if (!head && strcmp(header, "GET")) { status = 405; reason = "Method Not Allowed"; }
    char *host = NULL, *origin = NULL, *real = NULL;
    for (char *line = line_end + 2; line < end; ) {
        char *next = strstr(line, "\r\n");
        if (!next) return -1;
        *next = '\0';
        char *value = strchr(line, ':');
        if (!value || value == line) { status = 400; reason = "Bad Request"; break; }
        *value++ = '\0';
        for (const unsigned char *p = (unsigned char *)line; *p; p++)
            if (!isalnum(*p) && *p != '-') status = 400;
        while (*value == ' ' || *value == '\t') value++;
        char *tail = value + strlen(value);
        while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = '\0';
        char **field = !strcasecmp(line, "Host") ? &host
            : !strcasecmp(line, "Origin") ? &origin
            : !strcasecmp(line, "X-Real-IP") ? &real : NULL;
        if (field) {
            if (*field) status = 400;
            *field = value;
        }
        if (!strcasecmp(line, "Transfer-Encoding")
            || (!strcasecmp(line, "Content-Length") && strcmp(value, "0"))) status = 400;
        line = next + 2;
    }
    if (!Valid_host(host)) status = 400;
    if (status == 400) reason = "Bad Request";
    if (status == 200 && !strcmp(path, "/xpilot") && !head) {
        char expected[280];
        snprintf(expected, sizeof(expected), "http://%s", host);
        if (origin && strcmp(origin, front->origin ? front->origin : expected)) {
            status = 403; reason = "Forbidden";
        }
        if (front->proxy && !strcmp(front->proxy, request->address)) {
            if (!Normalize_address(real, request->address)) { status = 400; reason = "Bad Request"; }
        }
        if (status == 200) {
            /* Leave Upgrade and any coalesced game bytes for the WS transport. */
            sock_t socket = request->socket;
            request->active = false;
            front->upgrade(&socket, request->address, request->port, &request->endpoint);
            memset(request, 0, sizeof(*request));
            return 0;
        }
    }
    /* Only GET/HEAD are served, with no body and no persistent connection. */
    char discarded[WEB_HEADER];
    if (sock_read(&request->socket, discarded, (int)length) != (int)length) return -1;
    if (status == 200) {
        if (!Decode_path(path)) { status = 400; reason = "Bad Request"; }
        else {
            char index[] = "/index.html";
            char *file_path = !strcmp(path, "/") ? index : path;
            const char *mime = Mime(file_path);
            size_t size = 0;
            request->file = Open_asset(front, file_path, &size);
            if (!request->file) {
                status = size > WEB_FILE_LIMIT ? 413 : 404;
                reason = size > WEB_FILE_LIMIT ? "Content Too Large" : "Not Found";
            } else {
                Response(request, 200, "OK", mime, size);
                request->remaining = head ? 0 : size;
                return 0;
            }
        }
    }
    Response(request, status, reason, "text/plain", 0);
    return 0;
}

void Web_front_step(web_front_t *front)
{
    if (!front) return;
    size_t budget = WEB_FRAME_BUDGET;
    time_t now = time(NULL);
    for (size_t i = 0; i < WEB_PENDING && budget; i++) {
        web_request_t *request = &front->requests[front->cursor++ % WEB_PENDING];
        if (!request->active) continue;
        if (request->finishing) {
            /* Oversized headers can leave unread input. Finish output first,
             * then drain with a deadline so close cannot discard the response
             * by resetting the TCP connection. */
            int received = sock_read(&request->socket, request->output, sizeof(request->output));
            if (received > 0) budget -= MIN(budget, (size_t)received);
            if (!received || (received < 0 && !sock_error_is_temporary(&request->socket))
                || now - request->progress > 2) Request_close(request);
            continue;
        }
        if (now - request->opened > (request->responding ? 120 : 5)
            || now - request->progress > 10) { Request_close(request); continue; }
        if (!request->responding && Read_request(front, request) < 0) {
            Request_close(request);
            continue;
        }
        if (!request->active || !request->responding) continue;
        if (request->offset == request->length) {
            if (!request->remaining) { Request_close(request); continue; }
            size_t length = MIN(sizeof(request->output), request->remaining);
            request->length = fread(request->output, 1, length, request->file);
            request->offset = 0;
            if (request->length != length) { Request_close(request); continue; }
            request->remaining -= length;
        }
        size_t length = MIN(request->length - request->offset, budget);
        int written = sock_write(&request->socket, request->output + request->offset, (int)length);
        if (written > 0) {
            request->offset += (size_t)written;
            budget -= (size_t)written;
            request->progress = now;
            if (request->offset == request->length && !request->remaining) {
                if (sock_shutdown_write(&request->socket) == SOCK_IS_ERROR) Request_close(request);
                else request->finishing = true;
            }
        } else if (!written || !sock_error_is_temporary(&request->socket)) Request_close(request);
    }
}

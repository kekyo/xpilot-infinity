#include "utf8_files.h"
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
wchar_t *Xp_wide(const char *text)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!length) { errno = EILSEQ; return NULL; }
    wchar_t *wide = malloc((size_t)length * sizeof(wchar_t));
    if (!wide) { errno = ENOMEM; return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length)) {
        free(wide); errno = EILSEQ; return NULL;
    }
    return wide;
}

char *Xp_utf8(const wchar_t *text)
{
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    if (!length) { errno = EILSEQ; return NULL; }
    char *utf8 = malloc((size_t)length);
    if (!utf8) { errno = ENOMEM; return NULL; }
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8, length, NULL, NULL)) {
        free(utf8); errno = EILSEQ; return NULL;
    }
    return utf8;
}
#endif

FILE *Xp_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t *filename = Xp_wide(path), *access = Xp_wide(mode);
    FILE *file = filename && access ? _wfsopen(filename, access, _SH_DENYNO) : NULL;
    free(filename); free(access);
    return file;
#else
    return fopen(path, mode);
#endif
}

int Xp_open_read(const char *path)
{
#ifdef _WIN32
    wchar_t *filename = Xp_wide(path);
    if (!filename) return -1;
    int fd = -1;
    errno_t result = _wsopen_s(&fd, filename, _O_RDONLY | _O_BINARY, _SH_DENYNO, 0);
    free(filename);
    if (result) errno = result;
    return fd;
#else
    return open(path, O_RDONLY);
#endif
}

int Xp_access(const char *path, int mode)
{
#ifdef _WIN32
    wchar_t *filename = Xp_wide(path);
    if (!filename) return -1;
    errno_t result = _waccess_s(filename, mode);
    free(filename);
    if (result) { errno = result; return -1; }
    return 0;
#else
    return access(path, mode);
#endif
}

int Xp_rename(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t *source = Xp_wide(from), *destination = Xp_wide(to);
    int result = source && destination ? _wrename(source, destination) : -1;
    free(source); free(destination);
    return result;
#else
    return rename(from, to);
#endif
}

int Xp_remove(const char *path)
{
#ifdef _WIN32
    wchar_t *filename = Xp_wide(path);
    if (!filename) return -1;
    int result = _wremove(filename);
    free(filename);
    return result;
#else
    return remove(path);
#endif
}

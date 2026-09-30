#define _POSIX_C_SOURCE 200809L
#include "map_catalog.h"
#include "service_config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

bool map_catalog_name(const char *name)
{
    size_t length = strlen(name);
    if (!length || name[0] == '.' || !service_config_valid(name, length)) return false;
    for (const char *p = name; *p; p++)
        if ((unsigned char)*p < 32 || *p == '/' || *p == '\\' || *p == ':') return false;
    const char *extension = strrchr(name, '.');
    return extension && (!strcmp(extension, ".xp2") || !strcmp(extension, ".xp")
                         || !strcmp(extension, ".map"));
}

void map_catalog_clear(map_catalog *catalog)
{
    for (size_t i = 0; i < catalog->count; i++) free(catalog->names[i]);
    free(catalog->names);
    *catalog = (map_catalog){0};
}

static bool append(map_catalog *catalog, const char *name)
{
    char **names = realloc(catalog->names, (catalog->count + 1) * sizeof(char *));
    if (!names) return false;
    catalog->names = names;
    char *copy = malloc(strlen(name) + 1);
    if (!copy) return false;
    strcpy(copy, name);
    catalog->names[catalog->count++] = copy;
    return true;
}

static int compare(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

bool map_catalog_read(const char *directory, map_catalog *catalog)
{
    *catalog = (map_catalog){0};
    bool success = true;
#ifdef _WIN32
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, directory, -1, NULL, 0);
    if (!length) return false;
    wchar_t *pattern = calloc((size_t)length + 3, sizeof(wchar_t));
    if (!pattern) return false;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, directory, -1, pattern, length);
    wcscat(pattern, L"\\*");
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(pattern, &entry);
    free(pattern);
    if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        if (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName,
                                       -1, NULL, 0, NULL, NULL);
        if (!count) { success = false; break; }
        char *name = malloc((size_t)count);
        if (!name) { success = false; break; }
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1,
                            name, count, NULL, NULL);
        if (map_catalog_name(name)) success = append(catalog, name);
        free(name);
        if (!success) break;
    } while (FindNextFileW(search, &entry));
    if (success && GetLastError() != ERROR_NO_MORE_FILES) success = false;
    FindClose(search);
#else
    DIR *dir = opendir(directory);
    if (!dir) return false;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) success = false; break; }
        if (!map_catalog_name(entry->d_name)) continue;
        size_t size = strlen(directory) + strlen(entry->d_name) + 2;
        char *path = malloc(size);
        if (!path) { success = false; break; }
        snprintf(path, size, "%s/%s", directory, entry->d_name);
        struct stat st;
        int result = lstat(path, &st);
        free(path);
        if (result) { if (errno == ENOENT) continue; success = false; break; }
        if (S_ISREG(st.st_mode) && !append(catalog, entry->d_name)) { success = false; break; }
    }
    if (closedir(dir)) success = false;
#endif
    if (success && catalog->count > 1)
        qsort(catalog->names, catalog->count, sizeof(char *), compare);
    if (!success) map_catalog_clear(catalog);
    return success;
}

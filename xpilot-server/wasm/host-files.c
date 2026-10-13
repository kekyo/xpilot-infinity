/* Translate paths at the libc boundary so command line options and config files
 * have the same filesystem meaning. Each wrapper owns its temporary paths. */
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

EM_JS(char *, host_path, (const char *path), {
    return stringToNewUTF8(Module['mapPath'](UTF8ToString(path)));
});

extern FILE *__real_fopen(const char *, const char *);
extern int __real_open(const char *, int, ...);
extern int __real_access(const char *, int);
extern int __real_stat(const char *, struct stat *);
extern int __real_rename(const char *, const char *);
extern int __real_remove(const char *);

/** Open a host or packaged file, returning a caller-owned stream. */
FILE *__wrap_fopen(const char *path, const char *mode)
{
    char *mapped = host_path(path);
    FILE *result = __real_fopen(mapped, mode);
    free(mapped);
    return result;
}

/** Open a host file/directory, preserving the optional creation mode. */
int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    char *mapped = host_path(path);
    int result = __real_open(mapped, flags, mode);
    free(mapped);
    return result;
}

/** Check host or packaged file accessibility. */
int __wrap_access(const char *path, int mode)
{
    char *mapped = host_path(path);
    int result = __real_access(mapped, mode);
    free(mapped);
    return result;
}

/** Read host or packaged file metadata. */
int __wrap_stat(const char *path, struct stat *info)
{
    char *mapped = host_path(path);
    int result = __real_stat(mapped, info);
    free(mapped);
    return result;
}

/** Atomically replace saved host data using ordinary rename semantics. */
int __wrap_rename(const char *from, const char *to)
{
    char *source = host_path(from);
    char *destination = host_path(to);
    int result = __real_rename(source, destination);
    free(source);
    free(destination);
    return result;
}

/** Remove a host file. */
int __wrap_remove(const char *path)
{
    char *mapped = host_path(path);
    int result = __real_remove(mapped);
    free(mapped);
    return result;
}

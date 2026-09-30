#define _GNU_SOURCE
#include "settings_file_linux.h"
#include "service_config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

struct settings_file {
    int directory;
    int lock;
    int original;
    char *name;
    char *text;
    struct stat metadata;
};

static gboolean fail(GError **error, GIOErrorEnum code, const char *message)
{
    g_set_error_literal(error, G_IO_ERROR, code, message);
    return FALSE;
}

static gboolean io_error(GError **error, const char *message)
{
    int saved = errno;
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved), "%s: %s", message, g_strerror(saved));
    return FALSE;
}

static gboolean trusted(const struct stat *st, bool directory)
{
    return st->st_uid == geteuid() && !(st->st_mode & 0022)
        && (directory ? S_ISDIR(st->st_mode) : S_ISREG(st->st_mode) && st->st_nlink == 1);
}

static char *read_text(int fd, const struct stat *metadata, GError **error)
{
    if (metadata->st_size < 0 || metadata->st_size > 1024 * 1024) {
        fail(error, G_IO_ERROR_INVALID_DATA, "Configuration exceeds 1 MiB"); return NULL;
    }
    size_t length = (size_t)metadata->st_size, used = 0;
    char *text = g_malloc(length + 1);
    while (used < length) {
        ssize_t count = pread(fd, text + used, length - used, (off_t)used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            fail(error, G_IO_ERROR_FAILED, "Configuration changed while being read");
            g_free(text); return NULL;
        }
        used += (size_t)count;
    }
    text[length] = '\0';
    if (!service_config_valid(text, length)) {
        fail(error, G_IO_ERROR_INVALID_DATA, "Configuration must be UTF-8 without NUL or BOM");
        g_free(text); return NULL;
    }
    return text;
}

settings_file *settings_file_open(const char *path, const char *expected, GError **error)
{
    settings_file *file = g_new0(settings_file, 1);
    file->directory = file->lock = file->original = -1;
    char *directory = g_path_get_dirname(path);
    file->name = g_path_get_basename(path);
    file->directory = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    g_free(directory);
    if (file->directory < 0) { io_error(error, "Open configuration directory"); goto failed; }
    struct stat st;
    if (fstat(file->directory, &st)) { io_error(error, "Read directory metadata"); goto failed; }
    if (!trusted(&st, true)) {
        fail(error, G_IO_ERROR_PERMISSION_DENIED, "Configuration directory is not protected"); goto failed;
    }
    file->lock = openat(file->directory, ".xpilot-infinity-settings.lock",
                       O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (file->lock < 0) { io_error(error, "Open configuration lock"); goto failed; }
    if (fstat(file->lock, &st)) { io_error(error, "Read lock metadata"); goto failed; }
    if (!trusted(&st, false)) {
        fail(error, G_IO_ERROR_PERMISSION_DENIED, "Configuration lock is not protected"); goto failed;
    }
    if (flock(file->lock, LOCK_EX | LOCK_NB)) { io_error(error, "Another configuration update is active"); goto failed; }
    file->original = openat(file->directory, file->name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (file->original < 0) {
        if (errno != ENOENT) { io_error(error, "Open configuration"); goto failed; }
        if (strcmp(expected, "absent")) {
            fail(error, G_IO_ERROR_WRONG_ETAG, "Configuration was removed; reload before applying"); goto failed;
        }
        file->text = g_strdup("");
        return file;
    }
    if (fstat(file->original, &file->metadata)) { io_error(error, "Read configuration metadata"); goto failed; }
    if (!trusted(&file->metadata, false)) {
        fail(error, G_IO_ERROR_PERMISSION_DENIED, "Configuration is not a protected regular file"); goto failed;
    }
    file->text = read_text(file->original, &file->metadata, error);
    if (!file->text) goto failed;
    char *generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, file->text, -1);
    bool current = !strcmp(generation, expected);
    g_free(generation);
    if (!current) {
        fail(error, G_IO_ERROR_WRONG_ETAG, "Configuration changed; reload before applying"); goto failed;
    }
    return file;
failed:
    settings_file_close(file);
    return NULL;
}

const char *settings_file_text(const settings_file *file)
{
    return file->text;
}

static gboolean unchanged(settings_file *file, GError **error)
{
    struct stat current;
    if (fstatat(file->directory, file->name, &current, AT_SYMLINK_NOFOLLOW)) {
        if (errno == ENOENT && file->original < 0) return TRUE;
        return fail(error, G_IO_ERROR_WRONG_ETAG, "Configuration disappeared before saving");
    }
    const struct stat *before = &file->metadata;
    if (file->original < 0 || current.st_ino != before->st_ino || current.st_dev != before->st_dev
        || current.st_mode != before->st_mode || current.st_uid != before->st_uid
        || current.st_gid != before->st_gid || current.st_size != before->st_size
        || current.st_mtim.tv_sec != before->st_mtim.tv_sec || current.st_mtim.tv_nsec != before->st_mtim.tv_nsec
        || current.st_ctim.tv_sec != before->st_ctim.tv_sec || current.st_ctim.tv_nsec != before->st_ctim.tv_nsec)
        return fail(error, G_IO_ERROR_WRONG_ETAG, "Configuration changed before saving");
    char *text = read_text(file->original, &current, error);
    if (!text) return FALSE;
    bool same = !strcmp(text, file->text);
    g_free(text);
    return same || fail(error, G_IO_ERROR_WRONG_ETAG, "Configuration content changed before saving");
}

static gboolean copy_attributes(int original, int temporary, GError **error)
{
    ssize_t length = flistxattr(original, NULL, 0);
    if (length < 0 && errno == ENOTSUP) return TRUE;
    if (length < 0) return io_error(error, "List configuration attributes");
    if (!length) return TRUE;
    char *names = g_malloc((size_t)length);
    if (flistxattr(original, names, (size_t)length) != length) {
        g_free(names); return io_error(error, "Read configuration attributes");
    }
    gboolean ok = TRUE;
    for (char *name = names; name < names + length; name += strlen(name) + 1) {
        ssize_t size = fgetxattr(original, name, NULL, 0);
        if (size < 0) { ok = io_error(error, "Read configuration attribute size"); break; }
        void *value = g_malloc((size_t)size + 1);
        if (fgetxattr(original, name, value, (size_t)size) != size
            || fsetxattr(temporary, name, value, (size_t)size, 0))
            ok = io_error(error, "Preserve configuration attributes");
        g_free(value);
        if (!ok) break;
    }
    g_free(names);
    return ok;
}

static gboolean replace(settings_file *file, const char *name, const char *text,
                        bool configuration, GError **error)
{
    size_t length = strlen(text);
    if (!service_config_valid(text, length))
        return fail(error, G_IO_ERROR_INVALID_DATA, "Configuration must be UTF-8 without NUL or BOM");
    if (configuration && !unchanged(file, error)) return FALSE;
    char *temporary = g_strdup_printf(".xpilot-settings-%ld-%08x", (long)getpid(), g_random_int());
    int fd = openat(file->directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) { g_free(temporary); return io_error(error, "Create replacement configuration"); }
    gboolean ok = TRUE;
    size_t written = 0;
    while (written < length) {
        ssize_t count = write(fd, text + written, length - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = io_error(error, "Write replacement configuration"); break; }
        written += (size_t)count;
    }
    if (ok && configuration && file->original >= 0) {
        if (fchown(fd, file->metadata.st_uid, file->metadata.st_gid)
            || fchmod(fd, file->metadata.st_mode & 07777)) ok = io_error(error, "Preserve configuration permissions");
        if (ok) ok = copy_attributes(file->original, fd, error);
    } else if (ok && fchmod(fd, 0644)) ok = io_error(error, "Set configuration permissions");
    if (ok && fsync(fd)) ok = io_error(error, "Flush replacement configuration");
    if (close(fd) && ok) ok = io_error(error, "Close replacement configuration");
    if (ok && configuration) ok = unchanged(file, error);
    if (ok && renameat(file->directory, temporary, file->directory, name))
        ok = io_error(error, "Replace configuration");
    if (ok) {
        /* Replacement is already visible. Never misreport a saved configuration
         * as unchanged if the filesystem cannot flush the directory afterward. */
        if (fsync(file->directory)) g_warning("Configuration saved; directory sync failed: %s", g_strerror(errno));
    } else unlinkat(file->directory, temporary, 0);
    g_free(temporary);
    return ok;
}

void settings_file_close(settings_file *file)
{
    if (!file) return;
    if (file->original >= 0) close(file->original);
    if (file->lock >= 0) close(file->lock);
    if (file->directory >= 0) close(file->directory);
    g_free(file->name); g_free(file->text); g_free(file);
}

gboolean settings_file_record(settings_file *file, const char *generation, const char *detail, GError **error)
{
    char *text = g_strdup_printf("%s\n%s\n", generation, detail);
    gboolean ok = replace(file, ".xpilot-infinity-settings-result", text, false, error);
    g_free(text);
    return ok;
}

gboolean settings_file_commit(settings_file *file, const char *text, GError **error)
{
    return replace(file, file->name, text, true, error);
}

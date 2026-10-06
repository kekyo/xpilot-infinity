#define _POSIX_C_SOURCE 200809L
#include "settings_file_linux.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

int main(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("xpilot-settings-XXXXXX", &error);
    assert(directory && !error);
    char *path = g_build_filename(directory, "server.conf", NULL);
    assert(g_file_set_contents(path, "map: old.xp2\n", -1, &error));
    assert(!chmod(path, 0640));
    assert(!setxattr(path, "user.xpilot-test", "preserved", 9, 0));
    char *generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, "map: old.xp2\n", -1);
    settings_file *file = settings_file_open(path, generation, &error);
    assert(file && !error);
    settings_file *second = settings_file_open(path, generation, &error);
    assert(!second && error);
    g_clear_error(&error);
    assert(settings_file_record(file, generation, "Pending or interrupted operation", &error));
    char *record_path = g_build_filename(directory, ".xpilot-infinity-settings-result", NULL);
    char *record = NULL;
    assert(g_file_get_contents(record_path, &record, NULL, &error));
    assert(g_str_has_prefix(record, generation) && strstr(record, "Pending or interrupted operation"));
    g_free(record);
    assert(settings_file_commit(file, "map: 日本語.xp2\n", &error));
    settings_file_close(file);
    char *contents;
    assert(g_file_get_contents(path, &contents, NULL, &error));
    assert(!strcmp(contents, "map: 日本語.xp2\n"));
    g_free(contents);
    struct stat st;
    assert(!stat(path, &st) && (st.st_mode & 0777) == 0640);
    char attribute[16] = {0};
    assert(getxattr(path, "user.xpilot-test", attribute, sizeof(attribute)) == 9);
    assert(!strcmp(attribute, "preserved"));
    assert(!settings_file_open(path, generation, &error));
    g_clear_error(&error); g_free(generation);
    generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, "map: 日本語.xp2\n", -1);
    file = settings_file_open(path, generation, &error);
    assert(file && !error);
    assert(g_file_set_contents(path, "map: external.xp2\n", -1, &error));
    assert(!settings_file_commit(file, "map: stale.xp2\n", &error));
    g_clear_error(&error);
    settings_file_close(file);
    assert(g_file_get_contents(path, &contents, NULL, &error));
    assert(!strcmp(contents, "map: external.xp2\n"));
    g_free(contents); g_free(generation);
    assert(!unlink(path));
    assert(!symlink("/etc/passwd", path));
    assert(!settings_file_open(path, "absent", &error));
    g_clear_error(&error);
    assert(!unlink(path));
    file = settings_file_open(path, "absent", &error);
    assert(file && !error && !strcmp(settings_file_text(file), ""));
    assert(settings_file_commit(file, "map: first.xp2\n", &error));
    settings_file_close(file);
    assert(!unlink(path));
    char *lock = g_build_filename(directory, ".xpilot-infinity-settings.lock", NULL);
    assert(!unlink(lock));
    assert(!unlink(record_path));
    g_free(record_path);
    assert(!rmdir(directory));
    g_free(lock); g_free(path); g_free(directory);
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "config_editor.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char original[] = "XPILOT_SERVER_OPTIONS='-port 15345'\n";
static const char changed[] = "XPILOT_SERVER_OPTIONS='-port 15346'\n";
static const char later[] = "XPILOT_SERVER_OPTIONS='-port 15347'\n";
static const char generation[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char next[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

/* A real editor commonly saves by replacing the inode. */
static void save(const char *path, const char *text)
{
    char temporary[1024];
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    FILE *file = fopen(temporary, "wb");
    assert(file && fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file) && !rename(temporary, path));
}

int main(void)
{
    char directory[] = "/tmp/xpilot-editor-XXXXXX", error[256];
    assert(mkdtemp(directory));
    config_editor *editor = config_editor_open(directory, error);
    assert(editor);
    assert(config_editor_status(editor, generation) == EDITOR_NONE);
    assert(config_editor_begin(editor, original, generation));
    struct stat st;
    assert(!stat(directory, &st) && (st.st_mode & 0777) == 0700);
    assert(!stat(config_editor_path(editor), &st) && (st.st_mode & 0777) == 0600);
    assert(config_editor_status(editor, generation) == EDITOR_UNCHANGED);
    save(config_editor_path(editor), changed);
    assert(config_editor_status(editor, generation) == EDITOR_CHANGED);
    assert(!config_editor_snapshot(editor, next));
    assert(config_editor_status(editor, next) == EDITOR_CONFLICT);
    assert(config_editor_begin(editor, later, next));
    char *snapshot = config_editor_snapshot(editor, generation);
    assert(snapshot && !strcmp(snapshot, changed));
    save(config_editor_path(editor), later);
    assert(config_editor_accept(editor, snapshot, next));
    free(snapshot);
    assert(config_editor_status(editor, next) == EDITOR_CHANGED);
    config_editor_close(editor);
    editor = config_editor_open(directory, error);
    assert(editor && config_editor_status(editor, next) == EDITOR_CHANGED);
    snapshot = config_editor_snapshot(editor, next);
    assert(snapshot && !strcmp(snapshot, later));
    free(snapshot);
    assert(config_editor_discard(editor));
    assert(config_editor_status(editor, next) == EDITOR_NONE);
    /* The tray can disappear after the helper accepts an operation. */
    assert(config_editor_begin(editor, original, generation));
    save(config_editor_path(editor), changed);
    assert(config_editor_prepare(editor, changed, generation));
    save(config_editor_path(editor), later);
    config_editor_close(editor);
    editor = config_editor_open(directory, error);
    assert(editor && config_editor_reconcile(editor, original, generation));
    assert(config_editor_status(editor, generation) == EDITOR_CHANGED);
    assert(config_editor_reconcile(editor, changed, next));
    snapshot = config_editor_snapshot(editor, next);
    assert(snapshot && !strcmp(snapshot, later));
    free(snapshot);
    assert(config_editor_discard(editor));
    assert(config_editor_begin(editor, original, next));
    save(config_editor_path(editor), "\xff");
    assert(config_editor_status(editor, next) == EDITOR_INVALID);
    assert(!config_editor_snapshot(editor, next));
    assert(config_editor_discard(editor));
    assert(!symlink("/etc/passwd", config_editor_path(editor)));
    assert(!config_editor_snapshot(editor, next));
    assert(config_editor_discard(editor));
    config_editor_close(editor);
    char lock[1024];
    snprintf(lock, sizeof(lock), "%s/.lock", directory);
    assert(!unlink(lock) && !rmdir(directory));
    return 0;
}

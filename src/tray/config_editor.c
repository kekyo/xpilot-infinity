#define _GNU_SOURCE
#define _WIN32_WINNT 0x0600
#include "config_editor.h"
#include "service_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include "utf8_files.h"
#define COPY_NAME "xpilot-infinity-server.conf.txt"
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#define COPY_NAME "xpilot-infinity-server.txt"
#endif

struct config_editor {
    char *directory;
    char *copy_path;
    char error[256];
#ifdef _WIN32
    HANDLE folder;
    HANDLE lock;
    PSECURITY_DESCRIPTOR security;
#else
    int folder;
    int lock;
#endif
};

static bool fail(config_editor *editor, const char *message)
{
    snprintf(editor->error, sizeof(editor->error), "%s", message);
    return false;
}

static bool io_error(config_editor *editor, const char *operation)
{
#ifdef _WIN32
    snprintf(editor->error, sizeof(editor->error), "%s (Windows error %lu)", operation, (unsigned long)GetLastError());
#else
    snprintf(editor->error, sizeof(editor->error), "%s: %s", operation, strerror(errno));
#endif
    return false;
}

static char *path_join(const char *directory, const char *name)
{
    size_t length = strlen(directory) + strlen(name) + 2;
    char *path = malloc(length);
    if (path) snprintf(path, length, "%s/%s", directory, name);
    return path;
}

#ifdef _WIN32
static wchar_t *wide_path(config_editor *editor, const char *name)
{
    char *path = path_join(editor->directory, name);
    wchar_t *wide = path ? Xp_wide(path) : NULL;
    free(path);
    return wide;
}

static bool private_directory(config_editor *editor)
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return io_error(editor, "Read current user");
    DWORD needed;
    GetTokenInformation(token, TokenUser, NULL, 0, &needed);
    TOKEN_USER *user = malloc(needed);
    wchar_t *sid = NULL, *directory = Xp_wide(editor->directory);
    bool ok = user && directory && GetTokenInformation(token, TokenUser, user, needed, &needed)
        && ConvertSidToStringSidW(user->User.Sid, &sid);
    CloseHandle(token);
    if (ok) {
        wchar_t description[512];
        swprintf(description, 512, L"O:%lsD:P(A;OICI;FA;;;%ls)(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", sid, sid);
        ok = ConvertStringSecurityDescriptorToSecurityDescriptorW(description, SDDL_REVISION_1, &editor->security, NULL);
    }
    LocalFree(sid);
    if (ok) {
        SECURITY_ATTRIBUTES attributes = {sizeof(attributes), editor->security, FALSE};
        ok = CreateDirectoryW(directory, &attributes) || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    if (ok) {
        editor->folder = CreateFileW(directory, FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        ok = editor->folder != INVALID_HANDLE_VALUE;
    }
    if (ok) {
        BY_HANDLE_FILE_INFORMATION info;
        ok = GetFileInformationByHandle(editor->folder, &info)
            && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        PSECURITY_DESCRIPTOR security = NULL;
        PSID owner = NULL; PACL acl = NULL;
        if (ok) ok = GetSecurityInfo(editor->folder, SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, NULL, &acl, NULL, &security) == ERROR_SUCCESS;
        if (ok) ok = owner && EqualSid(owner, user->User.Sid) && acl;
        for (DWORD i = 0; ok && i < acl->AceCount; i++) {
            ACE_HEADER *header;
            ok = GetAce(acl, i, (void **)&header);
            if (!ok || (header->AceFlags & INHERIT_ONLY_ACE)) continue;
            if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
            if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) { ok = false; break; }
            ACCESS_ALLOWED_ACE *ace = (ACCESS_ALLOWED_ACE *)header;
            PSID allowed = &ace->SidStart;
            ok = EqualSid(allowed, user->User.Sid) || IsWellKnownSid(allowed, WinLocalSystemSid)
                || IsWellKnownSid(allowed, WinBuiltinAdministratorsSid);
        }
        LocalFree(security);
    }
    free(user); free(directory);
    return ok || fail(editor, "Editing directory must be private, owned by this user, and not a reparse point");
}
#endif

static char *read_file(config_editor *editor, const char *name, bool *missing)
{
    *missing = false;
    char *text = NULL;
    size_t length = 0;
#ifdef _WIN32
    wchar_t *path = wide_path(editor, name);
    if (!path) { fail(editor, "Cannot allocate editing path"); return NULL; }
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(path);
    if (file == INVALID_HANDLE_VALUE) {
        *missing = GetLastError() == ERROR_FILE_NOT_FOUND;
        io_error(editor, "Read editing file"); return NULL;
    }
    BY_HANDLE_FILE_INFORMATION info;
    bool valid = GetFileInformationByHandle(file, &info)
        && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        && info.nNumberOfLinks == 1 && !info.nFileSizeHigh && info.nFileSizeLow <= 1024 * 1024 + 128;
    if (valid) length = info.nFileSizeLow;
    if (valid) text = malloc(length + 1);
    DWORD received = 0;
    if (text && (!ReadFile(file, text, (DWORD)length, &received, NULL) || received != length)) {
        free(text); text = NULL;
    }
    CloseHandle(file);
#else
    int file = openat(editor->folder, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (file < 0) { *missing = errno == ENOENT; io_error(editor, "Read editing file"); return NULL; }
    struct stat st;
    bool valid = !fstat(file, &st) && S_ISREG(st.st_mode) && st.st_uid == geteuid()
        && st.st_nlink == 1 && st.st_size >= 0 && st.st_size <= 1024 * 1024 + 128;
    if (valid) length = (size_t)st.st_size;
    if (valid) text = malloc(length + 1);
    size_t used = 0;
    while (text && used < length) {
        ssize_t received = read(file, text + used, length - used);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) { free(text); text = NULL; break; }
        used += (size_t)received;
    }
    struct stat after;
    if (text && (fstat(file, &after) || st.st_size != after.st_size
        || st.st_mtim.tv_sec != after.st_mtim.tv_sec || st.st_mtim.tv_nsec != after.st_mtim.tv_nsec
        || st.st_ctim.tv_sec != after.st_ctim.tv_sec || st.st_ctim.tv_nsec != after.st_ctim.tv_nsec)) {
        free(text); text = NULL;
    }
    close(file);
#endif
    if (!text) fail(editor, "Editing file must be a readable, bounded regular file without links");
    else {
        text[length] = 0;
        if (strlen(text) != length) { free(text); text = NULL; fail(editor, "Editing file contains NUL bytes"); }
    }
    return text;
}

static bool write_file(config_editor *editor, const char *name, const char *text, bool replace)
{
    size_t length = strlen(text);
    bool ok;
#ifdef _WIN32
    wchar_t *target = wide_path(editor, name), *temporary = wide_path(editor, ".writing");
    if (!target || !temporary) { free(target); free(temporary); return fail(editor, "Cannot allocate editing path"); }
    /* The session lock owns this scratch name. A previous crash may leave it. */
    DeleteFileW(temporary);
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), editor->security, FALSE};
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    ok = file != INVALID_HANDLE_VALUE;
    if (ok) {
        DWORD written;
        ok = WriteFile(file, text, (DWORD)length, &written, NULL) && written == length && FlushFileBuffers(file);
        if (!CloseHandle(file)) ok = false;
        if (ok) ok = MoveFileExW(temporary, target, MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0));
    }
    if (!ok) io_error(editor, "Save editing file");
    DeleteFileW(temporary);
    free(target); free(temporary);
#else
    unlinkat(editor->folder, ".writing", 0);
    int file = openat(editor->folder, ".writing", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (file < 0) return io_error(editor, "Create editing file");
    ok = true;
    size_t used = 0;
    while (used < length) {
        ssize_t written = write(file, text + used, length - used);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ok = false; break; }
        used += (size_t)written;
    }
    if (ok && fsync(file)) ok = false;
    if (close(file)) ok = false;
    if (ok) ok = replace ? !renameat(editor->folder, ".writing", editor->folder, name)
                         : !linkat(editor->folder, ".writing", editor->folder, name, 0);
    if (!ok) io_error(editor, "Save editing file");
    unlinkat(editor->folder, ".writing", 0);
    if (ok && fsync(editor->folder)) ok = io_error(editor, "Flush editing directory");
#endif
    return ok;
}

config_editor *config_editor_open(const char *directory, char error[256])
{
    config_editor *editor = calloc(1, sizeof(*editor));
    if (!editor) { snprintf(error, 256, "Cannot allocate editing session"); return NULL; }
    editor->directory = malloc(strlen(directory) + 1);
    if (editor->directory) memcpy(editor->directory, directory, strlen(directory) + 1);
    editor->copy_path = path_join(directory, COPY_NAME);
#ifdef _WIN32
    editor->folder = editor->lock = INVALID_HANDLE_VALUE;
#else
    editor->folder = editor->lock = -1;
#endif
    if (!editor->directory || !editor->copy_path) { fail(editor, "Cannot allocate editing path"); goto failed; }
#ifdef _WIN32
    if (!private_directory(editor)) goto failed;
    wchar_t *path = wide_path(editor, ".lock");
    if (!path) { fail(editor, "Cannot allocate editing lock"); goto failed; }
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), editor->security, FALSE};
    editor->lock = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, &attributes,
        OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(path);
    if (editor->lock == INVALID_HANDLE_VALUE) { io_error(editor, "Another tray may own the editing session"); goto failed; }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(editor->lock, &info) || info.nNumberOfLinks != 1
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
        fail(editor, "Invalid editing session lock"); goto failed;
    }
#else
    if (directory[0] != '/') { fail(editor, "Editing directory must be absolute"); goto failed; }
    if (mkdir(directory, 0700) && errno != EEXIST) { io_error(editor, "Create editing directory"); goto failed; }
    editor->folder = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct stat st;
    if (editor->folder < 0 || fstat(editor->folder, &st) || st.st_uid != geteuid() || (st.st_mode & 0077)) {
        fail(editor, "Editing directory must be private and owned by this user"); goto failed;
    }
    editor->lock = openat(editor->folder, ".lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (editor->lock < 0 || fstat(editor->lock, &st) || !S_ISREG(st.st_mode)
        || st.st_uid != geteuid() || st.st_nlink != 1 || (st.st_mode & 0077)
        || flock(editor->lock, LOCK_EX | LOCK_NB)) {
        fail(editor, "Another tray owns the editing session, or its lock is not private"); goto failed;
    }
#endif
    editor->error[0] = error[0] = 0;
    return editor;
failed:
    snprintf(error, 256, "%s", editor->error);
    config_editor_close(editor);
    return NULL;
}

static bool valid_generation(const char *generation)
{
    return generation && (!strcmp(generation, "absent")
        || (strlen(generation) == 64 && strspn(generation, "0123456789abcdef") == 64));
}

static bool record_snapshot(config_editor *editor, const char *name, const char *snapshot, const char *generation)
{
    if (!valid_generation(generation) || !service_config_valid(snapshot, strlen(snapshot)))
        return fail(editor, "Cannot record an invalid UTF-8 snapshot or generation");
    size_t length = strlen(generation) + strlen(snapshot) + 2;
    char *baseline = malloc(length);
    if (!baseline) return fail(editor, "Cannot allocate editing baseline");
    snprintf(baseline, length, "%s\n%s", generation, snapshot);
    bool ok = write_file(editor, name, baseline, true);
    free(baseline);
    return ok;
}

static bool remove_file(config_editor *editor, const char *name)
{
#ifdef _WIN32
    wchar_t *path = wide_path(editor, name);
    if (!path) return fail(editor, "Cannot allocate editing path");
    bool ok = DeleteFileW(path) || GetLastError() == ERROR_FILE_NOT_FOUND;
    free(path);
#else
    bool ok = !unlinkat(editor->folder, name, 0) || errno == ENOENT;
    if (ok) ok = !fsync(editor->folder);
#endif
    return ok || io_error(editor, "Remove editing session file");
}

bool config_editor_accept(config_editor *editor, const char *snapshot, const char *generation)
{
    return record_snapshot(editor, ".baseline", snapshot, generation) && remove_file(editor, ".pending");
}

bool config_editor_prepare(config_editor *editor, const char *snapshot, const char *generation)
{
    return config_editor_status(editor, generation) == EDITOR_CHANGED
        && record_snapshot(editor, ".pending", snapshot, generation);
}

bool config_editor_reconcile(config_editor *editor, const char *contents, const char *generation)
{
    bool missing;
    char *pending = read_file(editor, ".pending", &missing);
    if (!pending) return missing;
    char *line = strchr(pending, '\n');
    if (line) *line++ = 0;
    bool ok = line && valid_generation(pending) && service_config_valid(line, strlen(line));
    if (ok && contents && !strcmp(line, contents) && valid_generation(generation)) {
        /* An old pending record must never replace a newer baseline. */
        char *baseline = read_file(editor, ".baseline", &missing);
        char *separator = baseline ? strchr(baseline, '\n') : NULL;
        if (separator) *separator = 0;
        if (separator && (!strcmp(baseline, pending) || !strcmp(baseline, generation)))
            ok = config_editor_accept(editor, line, generation);
        free(baseline);
    }
    free(pending);
    return ok || fail(editor, "Cannot recover the submitted editing snapshot; saved edits were retained");
}

bool config_editor_begin(config_editor *editor, const char *contents, const char *generation)
{
    bool missing;
    char *copy = read_file(editor, COPY_NAME, &missing);
    if (copy) { free(copy); return true; }
    if (!missing) return false;
    /* An interrupted initial creation can be resumed from its saved baseline. */
    char *baseline = read_file(editor, ".baseline", &missing);
    if (baseline) {
        char *line = strchr(baseline, '\n');
        if (line) *line++ = 0;
        bool ok = line && valid_generation(baseline) && service_config_valid(line, strlen(line))
            && write_file(editor, COPY_NAME, line, false);
        free(baseline);
        return ok || fail(editor, "Editing baseline is invalid; discard the session to start again");
    }
    if (!missing) return false;
    return config_editor_accept(editor, contents, generation) && write_file(editor, COPY_NAME, contents, false);
}

editor_state config_editor_status(config_editor *editor, const char *generation)
{
    bool missing;
    char *copy = read_file(editor, COPY_NAME, &missing);
    if (!copy) return missing ? EDITOR_NONE : EDITOR_INVALID;
    if (!service_config_valid(copy, strlen(copy))) { free(copy); fail(editor, "Saved copy must be UTF-8 without BOM"); return EDITOR_INVALID; }
    char *baseline = read_file(editor, ".baseline", &missing);
    char *line = baseline ? strchr(baseline, '\n') : NULL;
    if (line) *line++ = 0;
    editor_state state = EDITOR_CONFLICT;
    if (line && valid_generation(baseline) && generation && !strcmp(baseline, generation)
        && service_config_valid(line, strlen(line))) state = strcmp(copy, line) ? EDITOR_CHANGED : EDITOR_UNCHANGED;
    if (state == EDITOR_CONFLICT) fail(editor, "Shared configuration changed or the baseline is missing. Edits were retained; compare with the current configuration or discard to start again.");
    free(baseline); free(copy);
    return state;
}

char *config_editor_snapshot(config_editor *editor, const char *generation)
{
    if (config_editor_status(editor, generation) != EDITOR_CHANGED) return NULL;
    bool missing;
    char *copy = read_file(editor, COPY_NAME, &missing);
    if (copy && !service_config_valid(copy, strlen(copy))) { free(copy); copy = NULL; fail(editor, "Saved copy is not valid UTF-8"); }
    return copy;
}

bool config_editor_discard(config_editor *editor)
{
    const char *names[] = {COPY_NAME, ".baseline", ".pending"};
    for (size_t i = 0; i < 3; i++)
        if (!remove_file(editor, names[i])) return false;
    return true;
}

const char *config_editor_path(const config_editor *editor) { return editor->copy_path; }
const char *config_editor_error(const config_editor *editor) { return editor->error; }

void config_editor_close(config_editor *editor)
{
    if (!editor) return;
#ifdef _WIN32
    if (editor->lock != INVALID_HANDLE_VALUE) CloseHandle(editor->lock);
    if (editor->folder != INVALID_HANDLE_VALUE) CloseHandle(editor->folder);
    LocalFree(editor->security);
#else
    if (editor->lock >= 0) close(editor->lock);
    if (editor->folder >= 0) close(editor->folder);
#endif
    free(editor->directory); free(editor->copy_path); free(editor);
}

#define _WIN32_WINNT 0x0600
#include "settings_file_win32.h"
#include "service_paths_win32.h"
#include "service_config.h"
#include "map_catalog.h"
#include "utf8_files.h"
#include <aclapi.h>
#include <sddl.h>
#include <bcrypt.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct settings_file_win32 {
    wchar_t *path;
    HANDLE *parents;
    size_t parent_count;
    HANDLE lock;
    HANDLE source;
    BY_HANDLE_FILE_INFORMATION identity;
    char *text;
    char generation[65];
    PSECURITY_DESCRIPTOR security;
    PSID owner;
    PSID group;
    PACL dacl;
};

DWORD settings_win32_generation(const char *text, char generation[65])
{
    size_t length = strlen(text);
    if (!service_config_valid(text, length)) return ERROR_NO_UNICODE_TRANSLATION;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD size = 0, received = 0;
    unsigned char result[32];
    unsigned char *object = NULL;
    DWORD error = ERROR_INVALID_DATA;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&size, sizeof(size), &received, 0) < 0) goto done;
    object = malloc(size);
    if (!object) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
    if (BCryptCreateHash(algorithm, &hash, object, size, NULL, 0, 0) < 0
        || BCryptHashData(hash, (PUCHAR)text, (ULONG)length, 0) < 0
        || BCryptFinishHash(hash, result, sizeof(result), 0) < 0) goto done;
    for (size_t i = 0; i < sizeof(result); i++) {
        generation[i * 2] = "0123456789abcdef"[result[i] >> 4];
        generation[i * 2 + 1] = "0123456789abcdef"[result[i] & 15];
    }
    generation[64] = 0;
    error = ERROR_SUCCESS;
done:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    free(object);
    return error;
}

static DWORD read_handle(HANDLE handle, char **text, BY_HANDLE_FILE_INFORMATION *identity)
{
    *text = NULL;
    if (!GetFileInformationByHandle(handle, identity)) return GetLastError();
    if ((identity->dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        || identity->nNumberOfLinks != 1 || identity->nFileSizeHigh
        || identity->nFileSizeLow > 1024 * 1024) return ERROR_BAD_CONFIGURATION;
    char *contents = calloc((size_t)identity->nFileSizeLow + 1, 1);
    if (!contents) return ERROR_NOT_ENOUGH_MEMORY;
    DWORD received = 0;
    if (!ReadFile(handle, contents, identity->nFileSizeLow, &received, NULL)) {
        DWORD error = GetLastError(); free(contents); return error;
    }
    if (received != identity->nFileSizeLow || !service_config_valid(contents, received)) {
        free(contents); return ERROR_NO_UNICODE_TRANSLATION;
    }
    *text = contents;
    return ERROR_SUCCESS;
}

DWORD settings_win32_read(const wchar_t *path, char **text, char generation[65])
{
    *text = NULL;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    BY_HANDLE_FILE_INFORMATION info;
    DWORD error = read_handle(file, text, &info);
    CloseHandle(file);
    if (!error) error = settings_win32_generation(*text, generation);
    if (error) { free(*text); *text = NULL; }
    return error;
}

static bool trusted_sid(PSID sid, bool local_service)
{
    if (!sid) return false;
    if (IsWellKnownSid(sid, WinLocalSystemSid)
        || IsWellKnownSid(sid, WinBuiltinAdministratorsSid)
        || (local_service && IsWellKnownSid(sid, WinLocalServiceSid))) return true;
    /* Program Files inherits full control for Windows Modules Installer.
     * Its fixed service SID is also returned by sc.exe showsid TrustedInstaller. */
    PSID installer = NULL;
    if (!ConvertStringSidToSidW(
        L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", &installer)) return false;
    bool trusted = EqualSid(sid, installer) != FALSE;
    LocalFree(installer);
    return trusted;
}

static DWORD protected_handle(HANDLE handle, bool local_service)
{
    PSID owner = NULL;
    PACL acl = NULL;
    PSECURITY_DESCRIPTOR descriptor = NULL;
    DWORD error = GetSecurityInfo(handle, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, NULL, &acl, NULL, &descriptor);
    if (error) return error;
    if (!trusted_sid(owner, local_service) || !acl) error = ERROR_BAD_CONFIGURATION;
    for (DWORD i = 0; !error && i < acl->AceCount; i++) {
        ACE_HEADER *header = NULL;
        if (!GetAce(acl, i, (void **)&header)) { error = GetLastError(); break; }
        if (header->AceFlags & INHERIT_ONLY_ACE) continue;
        if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) { error = ERROR_BAD_CONFIGURATION; break; }
        ACCESS_ALLOWED_ACE *ace = (ACCESS_ALLOWED_ACE *)header;
        DWORD writes = GENERIC_ALL | GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA
            | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER;
        if ((ace->Mask & writes) && !trusted_sid(&ace->SidStart, local_service)) error = ERROR_BAD_CONFIGURATION;
    }
    LocalFree(descriptor);
    return error;
}

/* Keep every ancestor open without delete sharing. This prevents an ancestor
 * rename/reparse substitution while the fixed path is used for replacement. */
static DWORD hold_parents(settings_file_win32 *file, const wchar_t *path)
{
    if (wcslen(path) < 4 || path[1] != L':' || path[2] != L'\\'
        || wcschr(path + 2, L':') || wcschr(path, L'/')) return ERROR_BAD_CONFIGURATION;
    wchar_t *copy = _wcsdup(path);
    if (!copy) return ERROR_NOT_ENOUGH_MEMORY;
    file->parents = calloc(wcslen(path), sizeof(HANDLE));
    if (!file->parents) { free(copy); return ERROR_NOT_ENOUGH_MEMORY; }
    DWORD error = ERROR_SUCCESS;
    for (size_t i = 3; copy[i]; i++) {
        if (copy[i] != L'\\') continue;
        copy[i] = 0;
        HANDLE parent = CreateFileW(copy, FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        copy[i] = L'\\';
        if (parent == INVALID_HANDLE_VALUE) { error = GetLastError(); break; }
        file->parents[file->parent_count++] = parent;
        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(parent, &info)) { error = GetLastError(); break; }
        if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) { error = ERROR_BAD_CONFIGURATION; break; }
    }
    free(copy);
    return error;
}

void settings_win32_close(settings_file_win32 *file)
{
    if (!file) return;
    if (file->source != INVALID_HANDLE_VALUE) CloseHandle(file->source);
    if (file->lock != INVALID_HANDLE_VALUE) CloseHandle(file->lock);
    for (size_t i = 0; i < file->parent_count; i++) CloseHandle(file->parents[i]);
    LocalFree(file->security);
    free(file->parents); free(file->path); free(file->text); free(file);
}

settings_file_win32 *settings_win32_open(const char *expected, DWORD *error)
{
    settings_file_win32 *file = calloc(1, sizeof(*file));
    if (!file) { *error = ERROR_NOT_ENOUGH_MEMORY; return NULL; }
    file->source = file->lock = INVALID_HANDLE_VALUE;
    *error = service_configuration_path_win32(&file->path);
    if (!*error) *error = hold_parents(file, file->path);
    if (!*error && file->parent_count) *error = protected_handle(file->parents[file->parent_count - 1], true);
    if (*error) goto fail;
    size_t length = wcslen(file->path) + 16;
    wchar_t *lock_path = calloc(length, sizeof(wchar_t));
    if (!lock_path) { *error = ERROR_NOT_ENOUGH_MEMORY; goto fail; }
    swprintf(lock_path, length, L"%ls.lock", file->path);
    file->lock = CreateFileW(lock_path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
        0, NULL, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(lock_path);
    if (file->lock == INVALID_HANDLE_VALUE) { *error = GetLastError(); goto fail; }
    BY_HANDLE_FILE_INFORMATION lock_info;
    if (!GetFileInformationByHandle(file->lock, &lock_info)) { *error = GetLastError(); goto fail; }
    if ((lock_info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        || lock_info.nNumberOfLinks != 1) { *error = ERROR_BAD_CONFIGURATION; goto fail; }
    file->source = CreateFileW(file->path, GENERIC_READ | READ_CONTROL,
        FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file->source == INVALID_HANDLE_VALUE) { *error = GetLastError(); goto fail; }
    *error = protected_handle(file->source, true);
    if (!*error) *error = read_handle(file->source, &file->text, &file->identity);
    if (!*error) *error = settings_win32_generation(file->text, file->generation);
    if (!*error && strcmp(expected, file->generation)) *error = ERROR_REVISION_MISMATCH;
    if (!*error) *error = GetSecurityInfo(file->source, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &file->owner, &file->group, &file->dacl, NULL, &file->security);
    if (!*error) return file;
fail:
    settings_win32_close(file);
    return NULL;
}

const char *settings_win32_text(const settings_file_win32 *file) { return file->text; }

static DWORD replace(settings_file_win32 *file, const char *text, bool record)
{
    if (!service_config_valid(text, strlen(text))) return ERROR_NO_UNICODE_TRANSLATION;
    DWORD random[4];
    if (BCryptGenRandom(NULL, (PUCHAR)random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return ERROR_GEN_FAILURE;
    size_t length = wcslen(file->path) + 48;
    wchar_t *path = calloc(length, sizeof(wchar_t));
    if (!path) return ERROR_NOT_ENOUGH_MEMORY;
    swprintf(path, length, L"%ls.%08lx%08lx%08lx%08lx.tmp", file->path,
        random[0], random[1], random[2], random[3]);
    HANDLE temporary = CreateFileW(path, GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER,
        0, NULL, CREATE_NEW, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(path);
    if (temporary == INVALID_HANDLE_VALUE) return GetLastError();
    DWORD error = ERROR_SUCCESS, written = 0;
    if (!WriteFile(temporary, text, (DWORD)strlen(text), &written, NULL)) error = GetLastError();
    else if (written != strlen(text)) error = ERROR_WRITE_FAULT;
    SECURITY_DESCRIPTOR_CONTROL control;
    DWORD revision;
    if (!error && !GetSecurityDescriptorControl(file->security, &control, &revision)) error = GetLastError();
    if (!error) error = SetSecurityInfo(temporary, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION
        | ((control & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION),
        file->owner, file->group, file->dacl, NULL);
    if (!error && !FlushFileBuffers(temporary)) error = GetLastError();
    HANDLE current = INVALID_HANDLE_VALUE;
    if (!error && !record) {
        current = CreateFileW(file->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (current == INVALID_HANDLE_VALUE) error = GetLastError();
    }
    if (!error && !record) {
        BY_HANDLE_FILE_INFORMATION info;
        char *now = NULL;
        error = read_handle(current, &now, &info);
        if (!error && (info.dwVolumeSerialNumber != file->identity.dwVolumeSerialNumber
            || info.nFileIndexHigh != file->identity.nFileIndexHigh || info.nFileIndexLow != file->identity.nFileIndexLow
            || strcmp(now, file->text))) error = ERROR_REVISION_MISMATCH;
        free(now);
    }
    if (!error) {
        size_t target_length = wcslen(file->path) + (record ? 7 : 0);
        /* FileName is NUL-terminated; FileNameLength excludes the terminator. */
        size_t bytes = offsetof(FILE_RENAME_INFO, FileName) + (target_length + 1) * sizeof(wchar_t);
        FILE_RENAME_INFO *rename = calloc(1, bytes);
        if (!rename) error = ERROR_NOT_ENOUGH_MEMORY;
        else {
            rename->ReplaceIfExists = TRUE;
            rename->FileNameLength = (DWORD)(target_length * sizeof(wchar_t));
            memcpy(rename->FileName, file->path, wcslen(file->path) * sizeof(wchar_t));
            if (record) memcpy(rename->FileName + wcslen(file->path), L".result", 7 * sizeof(wchar_t));
            if (!record) {
                /* Windows cannot replace a destination with open readers.
                 * Validation is complete; keep the update lock and all parent
                 * guards while releasing our configuration read handles. */
                CloseHandle(current);
                current = INVALID_HANDLE_VALUE;
                if (file->source != INVALID_HANDLE_VALUE) CloseHandle(file->source);
                file->source = INVALID_HANDLE_VALUE;
            }
            if (!SetFileInformationByHandle(temporary, FileRenameInfo, rename, (DWORD)bytes)) error = GetLastError();
            free(rename);
        }
    }
    if (current != INVALID_HANDLE_VALUE) CloseHandle(current);
    if (error) {
        FILE_DISPOSITION_INFO disposition = {TRUE};
        SetFileInformationByHandle(temporary, FileDispositionInfo, &disposition, sizeof(disposition));
    }
    CloseHandle(temporary);
    return error;
}

DWORD settings_win32_map(const char *name, char **path)
{
    *path = NULL;
    if (!map_catalog_name(name)) return ERROR_INVALID_NAME;
    wchar_t *directory = NULL, *configuration = NULL;
    DWORD error = service_paths_win32(&directory, &configuration);
    free(configuration);
    if (error) return error;
    wchar_t *filename = Xp_wide(name);
    size_t length = wcslen(directory) + (filename ? wcslen(filename) : 0) + 16;
    wchar_t *full = calloc(length, sizeof(wchar_t));
    if (!filename || !full) error = ERROR_NOT_ENOUGH_MEMORY;
    if (!error) swprintf(full, length, L"%ls\\lib\\maps\\%ls", directory, filename);
    settings_file_win32 *guard = calloc(1, sizeof(*guard));
    if (!guard) error = ERROR_NOT_ENOUGH_MEMORY;
    else guard->source = guard->lock = INVALID_HANDLE_VALUE;
    if (!error) error = hold_parents(guard, full);
    if (!error) error = protected_handle(guard->parents[guard->parent_count - 1], false);
    if (!error) {
        guard->source = CreateFileW(full, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (guard->source == INVALID_HANDLE_VALUE) error = GetLastError();
        else {
            BY_HANDLE_FILE_INFORMATION info;
            if (!GetFileInformationByHandle(guard->source, &info)) error = GetLastError();
            else if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) error = ERROR_BAD_CONFIGURATION;
            if (!error) error = protected_handle(guard->source, false);
        }
    }
    if (!error) { *path = Xp_utf8(full); if (!*path) error = ERROR_NO_UNICODE_TRANSLATION; }
    settings_win32_close(guard);
    free(directory); free(filename); free(full);
    return error;
}

DWORD settings_win32_commit(settings_file_win32 *file, const char *text)
{
    return replace(file, text, false);
}

DWORD settings_win32_record(settings_file_win32 *file, const char *generation, const char *detail)
{
    size_t length = strlen(generation) + strlen(detail) + 3;
    char *text = malloc(length);
    if (!text) return ERROR_NOT_ENOUGH_MEMORY;
    snprintf(text, length, "%s\n%s\n", generation, detail);
    DWORD error = replace(file, text, true);
    free(text);
    return error;
}

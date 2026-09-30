#define _WIN32_WINNT 0x0600
#include "settings_transfer_win32.h"
#include "settings_file_win32.h"
#include "service_config.h"
#include <sddl.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static bool hex_id(const char *text)
{
    return strlen(text) == 64 && strspn(text, "0123456789abcdef") == 64;
}

static void object_name(const char token[65], wchar_t name[128])
{
    const wchar_t prefix[] = L"Local\\XPilotInfinitySettings-";
    size_t offset = wcslen(prefix);
    wmemcpy(name, prefix, offset);
    for (size_t i = 0; i <= 64; i++) name[offset + i] = (unsigned char)token[i];
}

static DWORD private_security(PSECURITY_DESCRIPTOR *security)
{
    *security = NULL;
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return GetLastError();
    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &needed);
    TOKEN_USER *user = malloc(needed);
    DWORD error = user ? ERROR_SUCCESS : ERROR_NOT_ENOUGH_MEMORY;
    wchar_t *sid = NULL;
    if (!error && (!GetTokenInformation(token, TokenUser, user, needed, &needed)
        || !ConvertSidToStringSidW(user->User.Sid, &sid))) error = GetLastError();
    CloseHandle(token);
    if (!error) {
        wchar_t descriptor[512];
        /* Only this user, SYSTEM and an elevated administrator can open it.
         * Readers never need write access to the captured snapshot. */
        swprintf(descriptor, 512, L"D:P(A;;GR;;;%ls)(A;;GR;;;BA)(A;;GR;;;SY)", sid);
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(descriptor,
            SDDL_REVISION_1, security, NULL)) error = GetLastError();
    }
    LocalFree(sid); free(user);
    return error;
}

DWORD settings_transfer_create(const char *text, char token[65], char hash[65], HANDLE *handle)
{
    *handle = NULL;
    DWORD error = settings_win32_generation(text, hash);
    if (error) return error;
    BYTE random[32];
    if (BCryptGenRandom(NULL, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return ERROR_GEN_FAILURE;
    for (size_t i = 0; i < sizeof(random); i++) {
        token[i * 2] = "0123456789abcdef"[random[i] >> 4];
        token[i * 2 + 1] = "0123456789abcdef"[random[i] & 15];
    }
    token[64] = 0;
    PSECURITY_DESCRIPTOR security;
    error = private_security(&security);
    if (error) return error;
    wchar_t name[128];
    object_name(token, name);
    DWORD length = (DWORD)strlen(text);
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), security, FALSE};
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes,
        PAGE_READWRITE, 0, sizeof(DWORD) + length, name);
    error = mapping ? (GetLastError() == ERROR_ALREADY_EXISTS ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS) : GetLastError();
    LocalFree(security);
    if (!error) {
        BYTE *view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(DWORD) + length);
        if (!view) error = GetLastError();
        else {
            memcpy(view, &length, sizeof(length));
            memcpy(view + sizeof(length), text, length);
            UnmapViewOfFile(view);
        }
    }
    if (error) { if (mapping) CloseHandle(mapping); }
    else *handle = mapping;
    return error;
}

DWORD settings_transfer_read(const char *token, const char *hash, char **text, HANDLE *handle)
{
    *text = NULL; *handle = NULL;
    if (!hex_id(token) || !hex_id(hash)) return ERROR_INVALID_PARAMETER;
    wchar_t name[128];
    object_name(token, name);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (!mapping) return GetLastError();
    DWORD length = 0, error = ERROR_SUCCESS;
    const BYTE *header = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(length));
    if (!header) error = GetLastError();
    else { memcpy(&length, header, sizeof(length)); UnmapViewOfFile(header); }
    if (!error && length > 1024 * 1024) error = ERROR_INVALID_DATA;
    if (!error) {
        const BYTE *view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(length) + length);
        if (!view) error = GetLastError();
        else {
            *text = calloc((size_t)length + 1, 1);
            if (!*text) error = ERROR_NOT_ENOUGH_MEMORY;
            else memcpy(*text, view + sizeof(length), length);
            UnmapViewOfFile(view);
        }
    }
    if (!error && !service_config_valid(*text, length)) error = ERROR_NO_UNICODE_TRANSLATION;
    char actual[65];
    if (!error) error = settings_win32_generation(*text, actual);
    if (!error && strcmp(actual, hash)) error = ERROR_REVISION_MISMATCH;
    if (error) { free(*text); *text = NULL; CloseHandle(mapping); }
    else *handle = mapping;
    return error;
}

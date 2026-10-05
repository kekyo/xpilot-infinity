/* Construct a transaction in a private temporary file so replacement can be
 * tested without administrator rights or an installed service. ACL validation
 * and the installed path are covered by their separate regression tests. */
#include "../settings_file_win32.c"
#include <assert.h>

int main(void)
{
    wchar_t directory[MAX_PATH], path[MAX_PATH], result[MAX_PATH + 8];
    assert(GetTempPathW(MAX_PATH, directory));
    assert(GetTempFileNameW(directory, L"xpt", 0, path));
    swprintf(result, MAX_PATH + 8, L"%ls.result", path);
    const char original[] = "map: ndh.xp2\nreportMeta: false\n";
    const char replacement[] = "map: circle2.xp2\nreportMeta: false\n";
    HANDLE seed = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(seed != INVALID_HANDLE_VALUE);
    DWORD written;
    assert(WriteFile(seed, original, sizeof(original) - 1, &written, NULL));
    assert(written == sizeof(original) - 1);
    CloseHandle(seed);

    settings_file_win32 *file = calloc(1, sizeof(*file));
    assert(file);
    file->path = _wcsdup(path);
    file->text = _strdup(original);
    assert(file->path && file->text);
    file->lock = INVALID_HANDLE_VALUE;
    file->source = CreateFileW(path, GENERIC_READ | READ_CONTROL,
        FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    assert(file->source != INVALID_HANDLE_VALUE);
    assert(GetFileInformationByHandle(file->source, &file->identity));
    assert(GetSecurityInfo(file->source, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &file->owner, &file->group, &file->dacl, NULL, &file->security) == ERROR_SUCCESS);
    assert(settings_win32_generation(original, file->generation) == ERROR_SUCCESS);
    assert(settings_win32_record(file, file->generation, "pending") == ERROR_SUCCESS);
    assert(settings_win32_commit(file, replacement) == ERROR_SUCCESS);
    assert(settings_win32_record(file, file->generation, "completed") == ERROR_SUCCESS);

    char *actual = NULL, generation[65];
    assert(settings_win32_read(path, &actual, generation) == ERROR_SUCCESS);
    assert(!strcmp(actual, replacement));
    free(actual);
    /* The original transaction cannot overwrite a newer generation. */
    assert(settings_win32_commit(file, original) == ERROR_REVISION_MISMATCH);
    assert(settings_win32_read(path, &actual, generation) == ERROR_SUCCESS);
    assert(!strcmp(actual, replacement));
    free(actual);
    assert(settings_win32_read(result, &actual, generation) == ERROR_SUCCESS);
    assert(strstr(actual, "\ncompleted\n"));
    free(actual);
    settings_win32_close(file);
    assert(DeleteFileW(path));
    assert(DeleteFileW(result));
    return 0;
}

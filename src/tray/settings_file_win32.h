#ifndef XPILOT_SETTINGS_FILE_WIN32_H
#define XPILOT_SETTINGS_FILE_WIN32_H
#include <windows.h>
#include <stdbool.h>
/** Exclusive shared-settings transaction. Owns its file and directory handles. */
typedef struct settings_file_win32 settings_file_win32;
/** Read a regular UTF-8 configuration without following a reparse point.
 * @param path Absolute, service-resolved path.
 * @param text Receives owned UTF-8 contents, released with free().
 * @param generation Receives 64 hexadecimal SHA-256 characters and a terminator.
 * @return Windows error code; missing or invalid files are errors.
 */
DWORD settings_win32_read(const wchar_t *path, char **text, char generation[65]);
/** Hash a valid, bounded UTF-8 snapshot.
 * @param text NUL-terminated contents.
 * @param generation Output SHA-256 buffer of 65 bytes.
 * @return Windows error code.
 */
DWORD settings_win32_generation(const char *text, char generation[65]);
/** Lock the fixed service configuration and capture its contents/permissions.
 * @param expected Expected SHA-256 generation.
 * @param error Receives a Windows error code.
 * @return Owned transaction, or NULL. No path is accepted from IPC callers.
 */
settings_file_win32 *settings_win32_open(const char *expected, DWORD *error);
/** Obtain captured UTF-8 configuration.
 * @param file Live transaction.
 * @return Borrowed text, valid until close.
 */
const char *settings_win32_text(const settings_file_win32 *file);
/** Atomically replace configuration, preserving its owner, group and DACL.
 * @param file Live transaction; generation is rechecked before replacement.
 * @param text Valid UTF-8 contents.
 * @return Windows error code. Service application is a separate operation.
 */
DWORD settings_win32_commit(settings_file_win32 *file, const char *text);
/** Persist a generation-keyed operation result atomically beside the configuration.
 * @param file Live transaction; retains its update lock through service application.
 * @param generation SHA-256 generation of the requested configuration.
 * @param detail UTF-8 pending/completed/failed operation description.
 * @return Windows error code. A pending record must precede configuration saving.
 */
DWORD settings_win32_record(settings_file_win32 *file, const char *generation, const char *detail);
/** Close an owned transaction without changing service state.
 * @param file Transaction, or NULL.
 */
void settings_win32_close(settings_file_win32 *file);
/** Resolve and validate a selected installed map using the registered service.
 * @param name UTF-8 catalog filename, without any directory separators.
 * @param path Receives an owned absolute UTF-8 path to free().
 * @return Windows error code; untrusted metadata is rejected.
 */
DWORD settings_win32_map(const char *name, char **path);
#endif

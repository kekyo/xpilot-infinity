#ifndef XPILOT_SERVICE_PATHS_WIN32_H
#define XPILOT_SERVICE_PATHS_WIN32_H
#include <windows.h>
/** Resolve the fixed shared configuration using the system known-folder API.
 * @param path Receives an allocated absolute path to free().
 * @return ERROR_SUCCESS or a Windows error.
 */
DWORD service_configuration_path_win32(wchar_t **path);
/** Resolve the registered server's installation, not the tray's directory.
 * @param directory Receives an allocated absolute directory to free().
 * @param configuration Receives the registered defaults path to free().
 * @return ERROR_SUCCESS, or the Windows error preventing resolution.
 */
DWORD service_paths_win32(wchar_t **directory, wchar_t **configuration);
/** Resolve the fixed helper beside the registered server executable.
 * @param path Receives an allocated absolute path to free().
 * @return ERROR_SUCCESS, or the Windows error preventing resolution.
 */
DWORD service_helper_path_win32(wchar_t **path);
#endif

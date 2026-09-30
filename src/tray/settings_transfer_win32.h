#ifndef XPILOT_SETTINGS_TRANSFER_WIN32_H
#define XPILOT_SETTINGS_TRANSFER_WIN32_H
#include <windows.h>
/** Publish a bounded immutable snapshot to a session-scoped, private memory object.
 * @param text Valid UTF-8 settings snapshot, at most 1 MiB.
 * @param token Receives a 64-character random object ID and terminator.
 * @param hash Receives the content SHA-256 and terminator.
 * @param handle Receives an owned handle; keep it open until the broker accepts it.
 * @return Windows error code. Other users cannot read the snapshot.
 */
DWORD settings_transfer_create(const char *text, char token[65], char hash[65], HANDLE *handle);
/** Capture and verify a published snapshot without accepting filesystem paths.
 * @param token Validated random object ID, without separators.
 * @param hash Expected SHA-256 chosen before authorization.
 * @param text Receives owned immutable text to free().
 * @param handle Receives a read-only owned handle, retained across UAC brokerage.
 * @return Windows error code. Invalid encoding, size and hash are rejected.
 */
DWORD settings_transfer_read(const char *token, const char *hash, char **text, HANDLE *handle);
#endif

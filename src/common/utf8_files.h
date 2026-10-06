#ifndef XPILOT_UTF8_FILES_H
#define XPILOT_UTF8_FILES_H
#include <stdio.h>
#ifdef _WIN32
#include <wchar.h>
/** Convert UTF-8 to UTF-16 without replacement characters.
 * @param text NUL-terminated UTF-8 string.
 * @return Owned string to free(), or NULL on invalid encoding/allocation failure.
 */
wchar_t *Xp_wide(const char *text);
/** Convert UTF-16 to UTF-8 without replacement characters.
 * @param text NUL-terminated UTF-16 string.
 * @return Owned string to free(), or NULL on invalid encoding/allocation failure.
 */
char *Xp_utf8(const wchar_t *text);
#endif
/** Open a file using a UTF-8 path.
 * @param path NUL-terminated UTF-8 filename.
 * @param mode Standard fopen mode, expressed in ASCII.
 * @return Owned FILE to fclose(), or NULL with errno set.
 */
FILE *Xp_fopen(const char *path, const char *mode);
/** Open a UTF-8 file for binary input.
 * @param path NUL-terminated UTF-8 filename.
 * @return Owned descriptor to close(), or -1 with errno set.
 */
int Xp_open_read(const char *path);
/** Check file access using a UTF-8 filename.
 * @param path NUL-terminated UTF-8 filename.
 * @param mode Standard access flags.
 * @return Zero on success, otherwise -1 with errno set.
 */
int Xp_access(const char *path, int mode);
/** Rename a UTF-8 filename using the platform's ordinary rename semantics.
 * @param from Existing filename.
 * @param to Destination filename.
 * @return Zero on success, otherwise -1 with errno set.
 */
int Xp_rename(const char *from, const char *to);
/** Remove a file using a UTF-8 filename.
 * @param path NUL-terminated UTF-8 filename.
 * @return Zero on success, otherwise -1 with errno set.
 */
int Xp_remove(const char *path);
#endif

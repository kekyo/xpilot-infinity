#ifndef XPILOT_SETTINGS_FILE_LINUX_H
#define XPILOT_SETTINGS_FILE_LINUX_H
#include <gio/gio.h>

/** Locked configuration snapshot. Owns all descriptors until close. */
typedef struct settings_file settings_file;
/** Open a trusted configuration and acquire its nonblocking update lock.
 * @param path Fixed absolute path selected by the helper, never by IPC callers.
 * @param expected Expected SHA-256 content generation, or "absent".
 * @param error Receives I/O, unsupported metadata, busy or generation conflict.
 * @return Owned transaction, or NULL. Directory/file must belong to the caller
 * and must not be group/world writable or symbolic links.
 */
settings_file *settings_file_open(const char *path, const char *expected, GError **error);
/** Read the captured contents.
 * @param file Live transaction.
 * @return Borrowed NUL-terminated UTF-8 contents.
 */
const char *settings_file_text(const settings_file *file);
/** Atomically save text while preserving owner, mode and extended attributes.
 * @param file Live transaction. Its generation is checked again before replacement.
 * @param text Valid UTF-8 configuration snapshot.
 * @param error Receives failure details; no partial configuration is exposed.
 * @return true on replacement. A later service restart may still fail separately.
 */
gboolean settings_file_commit(settings_file *file, const char *text, GError **error);
/** Persist an operation result beside the fixed configuration atomically.
 * @param file Live locked transaction.
 * @param generation Generation to which this result applies.
 * @param detail UTF-8 status, including pending/interrupted or application failure.
 * @param error Receives persistence failures.
 * @return true after durable replacement. Write a pending record before saving.
 */
gboolean settings_file_record(settings_file *file, const char *generation, const char *detail, GError **error);
/** Release the update lock and descriptors, without changing service state.
 * @param file Owned transaction; NULL is allowed.
 */
void settings_file_close(settings_file *file);
#endif

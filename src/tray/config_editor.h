#ifndef XPILOT_CONFIG_EDITOR_H
#define XPILOT_CONFIG_EDITOR_H
#include <stdbool.h>
/** Persistent, per-user editing session. Never owns a service or launches an editor. */
typedef struct config_editor config_editor;
/** State of the saved copy relative to its baseline and the shared configuration. */
typedef enum {
    EDITOR_NONE, /**< No saved editing copy exists. */
    EDITOR_UNCHANGED, /**< Saved copy is identical to its baseline. */
    EDITOR_CHANGED, /**< Saved changes can be explicitly applied. */
    EDITOR_CONFLICT, /**< Shared generation changed or the baseline is unavailable. */
    EDITOR_INVALID /**< Saved copy cannot be read as bounded UTF-8. */
} editor_state;
/** Open a private editing directory and retain its exclusive session lock.
 * @param directory Absolute UTF-8 per-user directory; its parent must exist.
 * @param error Receives an error description in a 256-byte buffer.
 * @return Owned session, or NULL. Existing copies are never overwritten here.
 */
config_editor *config_editor_open(const char *directory, char error[256]);
/** Create a copy and baseline if absent, or resume an existing saved copy.
 * @param editor Live session.
 * @param contents Current readable shared configuration.
 * @param generation Current shared generation (SHA-256 or "absent").
 * @return true when a copy exists; failure preserves existing edits.
 */
bool config_editor_begin(config_editor *editor, const char *contents, const char *generation);
/** Examine saved contents without treating editor exit as a save.
 * @param editor Live session.
 * @param generation Current shared generation, or NULL if unavailable.
 * @return Saved editing state; atomic editor replacement is supported.
 */
editor_state config_editor_status(config_editor *editor, const char *generation);
/** Capture only saved changes, refusing a stale baseline.
 * @param editor Live session.
 * @param generation Current shared generation.
 * @return Owned immutable UTF-8 snapshot to free(), or NULL with error detail.
 */
char *config_editor_snapshot(config_editor *editor, const char *generation);
/** Persist a submitted snapshot before handing it to the independent helper.
 * @param editor Live session.
 * @param snapshot Immutable saved contents submitted for authorization.
 * @param generation Shared generation on which this request is based.
 * @return true on durable storage; false means the request must not be sent.
 */
bool config_editor_prepare(config_editor *editor, const char *snapshot, const char *generation);
/** Recover an accepted operation after tray exit by comparing its exact contents.
 * @param editor Live session.
 * @param contents Current readable shared configuration.
 * @param generation Current shared generation.
 * @return true if no recovery was needed or it succeeded. Later edits are retained.
 */
bool config_editor_reconcile(config_editor *editor, const char *contents, const char *generation);
/** Record the exact accepted snapshot as the new baseline, preserving later saves.
 * @param editor Live session.
 * @param snapshot Contents the helper actually saved.
 * @param generation Shared generation returned by the helper.
 * @return true on durable baseline update. The editing copy is never replaced.
 */
bool config_editor_accept(config_editor *editor, const char *snapshot, const char *generation);
/** Discard the copy and baseline, leaving shared configuration untouched.
 * @param editor Live session. An external editor is not terminated.
 * @return true on success.
 */
bool config_editor_discard(config_editor *editor);
/** Get the absolute UTF-8 editing-copy path for the normal-user default editor.
 * @param editor Live session.
 * @return Borrowed path valid until close.
 */
const char *config_editor_path(const config_editor *editor);
/** Get the last operation's error detail.
 * @param editor Live session.
 * @return Borrowed UTF-8 diagnostic.
 */
const char *config_editor_error(const config_editor *editor);
/** Release resources, preserving saved edits and the baseline across tray restarts.
 * @param editor Owned session, or NULL.
 */
void config_editor_close(config_editor *editor);
#endif

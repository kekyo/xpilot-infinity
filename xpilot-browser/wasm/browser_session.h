#ifndef XP_BROWSER_SESSION_H
#define XP_BROWSER_SESSION_H
#include "record_session.h"
/** Start nonblocking admission on Module.xpilotSocket. Nickname is copied.
 * @param nickname UTF-8 player name. @return Zero on success, -1 on failure. */
int xp_browser_session_begin(const char *nickname);
/** Advance admission once. @return 0 pending, 1 confirmed, -1 failed. */
int xp_browser_session_step(void);
/** @return Borrowed stable diagnostic, valid until the next begin. */
const char *xp_browser_session_error(void);
/** Destroy admission and any session not transferred to gameplay. */
void xp_browser_session_close(void);
/** Transfer the admitted session to the caller; confirmation remains borrowed.
 * @param confirmation Receives borrowed first gameplay record.
 * @param length Receives record size. @param version Receives negotiated version.
 * @return Owned session, or NULL before successful admission. */
record_session_t *Browser_session_take(const char **confirmation, size_t *length,
                                       unsigned *version);
/** Wrap the current Module.xpilotSocket. Caller owns the returned transport. */
record_transport_t *Browser_transport_create(void);
#endif

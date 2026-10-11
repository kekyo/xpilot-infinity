#ifndef XP_BROWSER_GAME_H
#define XP_BROWSER_GAME_H
/** Initialize options, SDL and assets once. @return Zero on success. */
int xp_game_init(void);
/** Adopt admitted session. @param nickname Player name. @return Zero on success. */
int xp_game_begin(const char *nickname);
/** Advance once. @return 0 starting, 1 playing, 2 awaiting assets, -1 ended. */
int xp_game_step(void);
/** Send one UTF-8 chat message. @return Zero or positive on success. */
int xp_game_chat(const char *text);
/** Stop the game, release all native client resources. */
void xp_game_stop(void);
/** Request a logical canvas size. @return Zero on success. */
int xp_game_resize(int width, int height);
/** @return The last processed server frame number. */
long xp_game_frame(void);
/** @return Current ship coordinate on the requested axis (zero x, one y). */
int xp_game_position(int axis);
/** @return Last server-confirmed ship heading. */
int xp_game_heading(void);
/** @return Cumulative shot particles received by this instance. */
int xp_game_shots(void);
/** @return Borrowed latest chat line, valid until the next game step. */
const char *xp_game_message(void);
/** @return Borrowed map data URL while waiting for assets, otherwise empty. */
const char *xp_game_map_url(void);
/** Finish map initialization after assets arrive. @return Zero on success. */
int xp_game_assets_ready(void);
/** Begin token-based resumption on the newly installed browser socket. */
int xp_game_resume_begin(void);
/** Advance resumption: 0 pending, 1 resumed, -1 rejected. */
int xp_game_resume_step(void);
/** Advance orderly departure: 0 pending, 1 acknowledged or closed. */
int xp_game_quit_step(void);
/** Release held game keys after focus loss or suspension. */
void xp_game_clear_input(void);
#endif

#ifndef XP_BROWSER_GAME_H
#define XP_BROWSER_GAME_H
/** Initialize options, SDL and assets once. @return Zero on success. */
int xp_game_init(void);
/** Adopt admitted session. @param nickname Player name. @return Zero on success. */
int xp_game_begin(const char *nickname);
/** Advance once. @return 0 starting, 1 playing, 2 awaiting assets, -1 ended. */
int xp_game_step(void);
/** Send one UTF-8 chat message. @param text Borrowed null-terminated text. @return Zero or positive on success. */
int xp_game_chat(const char *text);
/** Stop the game, release all native client resources. */
void xp_game_stop(void);
/** Request a logical canvas size. @param width CSS width. @param height CSS height. @return Zero on success. */
int xp_game_resize(int width, int height);
/** @return The last processed server frame number. */
long xp_game_frame(void);
/** @param axis Coordinate selector: zero x, one y. @return Current ship coordinate. */
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
/** Begin token-based resumption on the newly installed browser socket. @return Zero on success, -1 on failure. */
int xp_game_resume_begin(void);
/** Advance resumption: 0 pending, 1 resumed, -1 rejected. */
int xp_game_resume_step(void);
/** Advance orderly departure: 0 pending, 1 acknowledged or closed. */
int xp_game_quit_step(void);
/** Release held game keys after focus loss or suspension. */
void xp_game_clear_input(void);
/** @return Number of currently held game actions. */
int xp_game_held(void);
/** @return Current logical game viewport width. */
int xp_game_width(void);
/** Evict Unicode bitmaps on display-density changes. @return Renderer status. */
int xp_game_text_scale_changed(void);
/** Serialize native options to the browser settings path. @return Zero on success. */
int xp_game_save_settings(void);
/** Retain CPU graphics resources after context loss. @return Renderer status. */
int xp_game_context_lost(void);
/** Rebuild retained graphics resources after context restoration. @return Renderer status. */
int xp_game_context_restored(void);
/** Set relative steering. @param enabled Boolean requested state. @return Zero on success. */
int xp_game_pointer(int enabled);
/** Request full screen from a user gesture. @param enabled Boolean requested state. @return Zero on success. */
int xp_game_fullscreen(int enabled);
#endif

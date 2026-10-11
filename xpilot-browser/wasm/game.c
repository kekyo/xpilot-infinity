#include "xpclient_sdl.h"
#include "sdlinit.h"
#include "browser_session.h"
#include "session_connection.h"
#include "game.h"
#include <emscripten.h>

static int phase;
static int initialized;
static double deadline;
static GameLoopState loop;

const char *Program_name(void) { return "xpilot-browser"; }
int xp_game_init(void)
{
    init_error("xpilot-browser");
    seedMT((unsigned)time(NULL));
    memset(&connectParam, 0, sizeof(connectParam));
    connectParam.team = TEAM_NOT_SET;
    connectParam.game_transport = GAME_TRANSPORT_WEBSOCKET;
    connectParam.contact_transport = GAME_TRANSPORT_WEBSOCKET;
    strlcpy(connectParam.user_name, "browser", sizeof(connectParam.user_name));
    strlcpy(connectParam.disp_name, "WebGL", sizeof(connectParam.disp_name));
    Store_default_options();
    Store_talk_macro_options();
    Store_key_options();
    Store_sdlinit_options();
    Store_sdlgui_options();
    Store_radar_options();
    memset(&xpArgs, 0, sizeof(xpArgs));
    char name[] = "xpilot-browser";
    char *arguments[] = {name, NULL};
    int count = 1;
    FILE *settings = fopen("/home/web_user/.xpilotrc", "a");
    if (settings) fclose(settings);
    Parse_options(&count, arguments);
    if (Init_window()) return -1;
    initialized = 1;
    return 0;
}
int xp_game_begin(const char *nickname)
{
    strlcpy(connectParam.nick_name, nickname, sizeof(connectParam.nick_name));
    const char *confirmation;
    size_t length;
    unsigned version;
    record_session_t *session = Browser_session_take(&confirmation, &length, &version);
    if (!session) return -1;
    if (Session_connection_stage_game(session, confirmation, length) < 0) return -1;
    if (Client_init("Content server", version)
        || Net_init("Content server", 0, GAME_TRANSPORT_WEBSOCKET)
        || Net_verify(connectParam.user_name, connectParam.nick_name, connectParam.disp_name)) return -1;
    phase = 1;
    deadline = emscripten_get_now() + 30000;
    return 0;
}
int xp_game_step(void)
{
    if (phase < 1) return -1;
    if (phase < 3 && emscripten_get_now() > deadline) return -1;
    if (phase == 1) {
        int result = Net_setup_step();
        if (result <= 0) return result;
        phase = 4;
        return 2;
    }
    if (phase == 4) return 2;
    if (phase == 2) {
        int result = Net_start_step();
        if (result <= 0) return result;
        if (Client_start() || Game_loop_prepare(&loop) != GAME_LOOP_CONTINUE) return -1;
        phase = 3;
    }
    return Game_loop_step(&loop, 1) == GAME_LOOP_CONTINUE ? 1 : -1;
}
int xp_game_chat(const char *text) { return Net_talk((char *)text); }
void xp_game_stop(void)
{
    if (!initialized) return;
    Net_cleanup();
    Client_cleanup();
    initialized = 0;
    phase = 0;
}
int xp_game_resize(int width, int height) { return Resize_Window(width, height); }
long xp_game_frame(void) { return last_loops; }
int xp_game_position(int axis) { return axis == 0 ? selfPos.x : selfPos.y; }
int xp_game_heading(void) { return heading; }
int xp_game_shots(void)
{
    return (int)received_fastshots;
}
const char *xp_game_message(void) { return phase == 3 && TalkMsg[0] ? TalkMsg[0]->txt : ""; }
const char *xp_game_map_url(void) { return phase == 4 ? Setup->data_url : ""; }
int xp_game_assets_ready(void)
{
    if (phase != 4 || Client_setup()) return -1;
    phase = 2;
    deadline = emscripten_get_now() + 30000;
    return 0;
}

int xp_game_resume_begin(void) { return Net_session_resume_begin(Browser_transport_create()); }
int xp_game_resume_step(void) { return Net_session_resume_step(); }
int xp_game_quit_step(void) { return Net_session_quit_step(); }
void xp_game_clear_input(void) { Key_clear_counts(); }

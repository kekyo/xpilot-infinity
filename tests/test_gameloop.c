#include "test_helpers.h"

#include "gameloop.h"

#include <stdarg.h>

int maxMouseTurnsPS;

static int net_input_result;
static int net_input_calls;
static int net_flush_result;
static int net_flush_calls;
static socket_handle_t net_fd_result;
static int net_fd_calls;
static int net_key_change_calls;
static int pointer_interval_calls;
static int game_input_result;
static int game_input_calls;
static int warning_calls;
static int error_calls;

int Net_input(void)
{
    net_input_calls++;
    return net_input_result;
}

int Net_flush(void)
{
    net_flush_calls++;
    return net_flush_result;
}

socket_handle_t Net_fd(void)
{
    net_fd_calls++;
    return net_fd_result;
}

void Net_key_change(void)
{
    net_key_change_calls++;
}

int Client_check_pointer_move_interval(void)
{
    pointer_interval_calls++;
    return 0;
}

int Game_input_process_batch(void)
{
    game_input_calls++;
    return game_input_result;
}

void warn(const char *format, ...)
{
    (void)format;
    warning_calls++;
}

void error(const char *format, ...)
{
    (void)format;
    error_calls++;
}

static void Reset_test_state(void)
{
    maxMouseTurnsPS = 0;
    net_input_result = 1;
    net_input_calls = 0;
    net_flush_result = 0;
    net_flush_calls = 0;
    net_fd_result = (socket_handle_t)37;
    net_fd_calls = 0;
    net_key_change_calls = 0;
    pointer_interval_calls = 0;
    game_input_result = 0;
    game_input_calls = 0;
    warning_calls = 0;
    error_calls = 0;
}

static int Test_prepare_initializes_schedulable_state(void)
{
    GameLoopState state = {SOCK_FD_INVALID, -1};

    Reset_test_state();
    net_input_result = 2;
    TEST_CHECK(Game_loop_prepare(&state) == GAME_LOOP_CONTINUE);
    TEST_CHECK(state.network_fd == (socket_handle_t)37);
    TEST_CHECK(state.previous_network_result == 2);
    TEST_CHECK(net_input_calls == 1);
    TEST_CHECK(game_input_calls == 1);
    TEST_CHECK(net_flush_calls == 1);
    TEST_CHECK(net_fd_calls == 1);
    TEST_CHECK(net_key_change_calls == 1);
    return 0;
}

static int Test_step_processes_pending_work_and_refreshes_fd(void)
{
    GameLoopState state = {(socket_handle_t)37, 2};

    Reset_test_state();
    maxMouseTurnsPS = 60;
    net_input_result = 1;
    net_fd_result = (socket_handle_t)91;
    TEST_CHECK(Game_loop_step(&state, 0) == GAME_LOOP_CONTINUE);
    TEST_CHECK(state.network_fd == (socket_handle_t)91);
    TEST_CHECK(state.previous_network_result == 1);
    TEST_CHECK(pointer_interval_calls == 1);
    TEST_CHECK(net_input_calls == 1);
    TEST_CHECK(game_input_calls == 1);
    TEST_CHECK(net_flush_calls == 1);
    TEST_CHECK(net_fd_calls == 1);

    TEST_CHECK(Game_loop_step(&state, 0) == GAME_LOOP_CONTINUE);
    TEST_CHECK(net_input_calls == 1);
    TEST_CHECK(game_input_calls == 2);
    TEST_CHECK(net_flush_calls == 2);
    TEST_CHECK(net_fd_calls == 2);
    return 0;
}

static int Test_step_reports_each_stop_condition(void)
{
    GameLoopState state = {(socket_handle_t)37, 1};

    Reset_test_state();
    TEST_CHECK(Game_loop_step(NULL, 0) == GAME_LOOP_STOP);

    Reset_test_state();
    net_input_result = -1;
    TEST_CHECK(Game_loop_step(&state, 1) == GAME_LOOP_STOP);
    TEST_CHECK(warning_calls == 1);
    TEST_CHECK(game_input_calls == 0);
    TEST_CHECK(net_flush_calls == 0);

    Reset_test_state();
    state.previous_network_result = 1;
    game_input_result = 1;
    TEST_CHECK(Game_loop_step(&state, 0) == GAME_LOOP_STOP);
    TEST_CHECK(game_input_calls == 1);
    TEST_CHECK(net_flush_calls == 0);

    Reset_test_state();
    state.previous_network_result = 1;
    net_flush_result = -1;
    TEST_CHECK(Game_loop_step(&state, 0) == GAME_LOOP_STOP);
    TEST_CHECK(net_flush_calls == 1);
    TEST_CHECK(error_calls == 1);

    Reset_test_state();
    state.previous_network_result = 1;
    net_fd_result = SOCK_FD_INVALID;
    TEST_CHECK(Game_loop_step(&state, 0) == GAME_LOOP_CONTINUE);
    TEST_CHECK(net_fd_calls == 1);
    TEST_CHECK(error_calls == 0);
    TEST_CHECK(Game_loop_prepare(&state) == GAME_LOOP_CONTINUE);
    return 0;
}

int main(void)
{
    if (Test_prepare_initializes_schedulable_state() != 0
        || Test_step_processes_pending_work_and_refreshes_fd() != 0
        || Test_step_reports_each_stop_condition() != 0) {
        return 1;
    }
    return 0;
}

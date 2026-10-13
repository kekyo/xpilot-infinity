#include "test_helpers.h"
#include "xpserver.h"

static rules_t test_rules;
static world_t test_world = {.rules = &test_rules};
world_t *world = &test_world;

/* Drive the real interception calculation without a running game loop. */
double Robot_ram_object(player_t *pl, object_t *object);

void End_game(void)
{
    abort();
}

void Thrust(player_t *pl, bool on)
{
    (void)pl;
    (void)on;
}

double Wrap_cfindDir(int dx, int dy)
{
    return atan2(dy, dx) * RES / (2 * M_PI);
}

int main(void)
{
    player_t player = {0};
    object_t target = {0};
    robot_data_t robot = {0};
    player.robot_data_ptr = &robot;
    player.power = 1.25;
    player.mass = 2.5;
    target.pos.cx = 20 * CLICK;

    double time = Robot_ram_object(&player, &target);
    TEST_CHECK(isfinite(time));
    TEST_CHECK(fabs(time - sqrt(80.0)) < 0.001);

    /* Large squared distances must never be converted to an integer when
     * checking convergence. Sanitizers catch that conversion before the fix. */
    target.pos.cx = 10000 * CLICK;
    player.power = 125.0;
    time = Robot_ram_object(&player, &target);
    TEST_CHECK(isfinite(time));
    TEST_CHECK(fabs(time - 20.0) < 0.001);
    return 0;
}

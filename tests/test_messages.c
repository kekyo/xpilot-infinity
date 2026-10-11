#include "test_helpers.h"
#include "xpclient.h"

selection_t selection;
static setup_t test_setup;
setup_t *Setup = &test_setup;
static other_t test_self;
other_t *self = &test_self;
other_t *Others;
int num_others;
unsigned version;
homebase_t *bases;
int num_bases;
long end_loops;
double clientFPS;
instruments_t instruments;
int num_playing_teams;

void Add_rank_Kill(char *name)
{
    (void)name;
    abort();
}

void Add_rank_Death(char *name)
{
    (void)name;
    abort();
}

/* These tests use ordinary chat and neutral game messages, with no players. */
other_t *Other_by_name(const char *name, bool show_error)
{
    (void)name;
    (void)show_error;
    return NULL;
}

other_t *Other_by_id(int id)
{
    (void)id;
    return NULL;
}

int main(void)
{
    maxMessages = 3;
    TEST_CHECK(Alloc_msgs() == 0);

    selection.draw.state = SEL_NONE;
    Add_message("original [pilot]");
    selection.draw.state = SEL_PENDING;
    Add_message("queued [pilot]");
    Add_message("A game event");
    TEST_CHECK(strcmp(TalkMsg[0]->txt, "original [pilot]") == 0);
    TEST_CHECK(strcmp(TalkMsg_pending[0]->txt, "queued [pilot]") == 0);
    TEST_CHECK(GameMsg[0]->len == 0);
    TEST_CHECK(strcmp(GameMsg_pending[0]->txt, "A game event") == 0);

    selection.draw.state = SEL_SELECTED;
    Add_pending_messages();
    TEST_CHECK(strcmp(TalkMsg[0]->txt, "queued [pilot]") == 0);
    TEST_CHECK(strcmp(GameMsg[0]->txt, "A game event") == 0);
    TEST_CHECK(TalkMsg_pending[0]->len == 0);
    TEST_CHECK(GameMsg_pending[0]->len == 0);

    selection.talk.state = SEL_EMPHASIZED;
    selection.keep_emphasizing = true;
    Add_message("selected [pilot]");
    TEST_CHECK(selection.talk.state == SEL_NONE);
    TEST_CHECK(selection.draw.state == SEL_EMPHASIZED);
    TEST_CHECK(selection.draw.y1 == 0 && selection.draw.y2 == 0);
    Add_message("next [pilot]");
    TEST_CHECK(selection.draw.y1 == 1 && selection.draw.y2 == 1);
    Add_message("last [pilot]");
    Add_message("beyond selection [pilot]");
    TEST_CHECK(selection.draw.state == SEL_SELECTED);

    Free_msgs();
    return 0;
}

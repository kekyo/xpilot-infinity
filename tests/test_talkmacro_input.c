#include "test_helpers.h"
#include "xpclient.h"

setup_t *Setup;
other_t *self;
bool snooping;
short lock_id;
int eyesId;
static bool short_read;
static char sent[MSG_LEN];

other_t *Other_by_id(int id) { (void)id; return NULL; }
int Net_talk(char *message)
{
    strlcpy(sent, message, sizeof(sent));
    return 0;
}

size_t __real_fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t __wrap_fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    return __real_fread(ptr, size, short_read && count > 0 ? count - 1 : count,
                        stream);
}

int main(void)
{
    char name[] = "/tmp/xpilot-macro-XXXXXX";
    int fd = mkstemp(name);
    TEST_CHECK(fd >= 0);
    const char contents[] = "macro text";
    TEST_CHECK(write(fd, contents, sizeof(contents) - 1) == sizeof(contents) - 1);
    TEST_CHECK(close(fd) == 0);
    char macro[256];
    snprintf(macro, sizeof(macro), "#f[%s]", name);
    talk_fast_msgs[0] = macro;
    TEST_CHECK(Talk_macro(0) == 0);
    TEST_CHECK(strcmp(sent, contents) == 0);

    /* A file can shrink between seeking to its end and reading it. */
    sent[0] = '\0';
    short_read = true;
    TEST_CHECK(Talk_macro(0) == 0);
    TEST_CHECK(sent[0] == '\0');
    TEST_CHECK(unlink(name) == 0);
    TEST_CHECK(Talk_macro(0) == 0);
    TEST_CHECK(sent[0] == '\0');
    return 0;
}

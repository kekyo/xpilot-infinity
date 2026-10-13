#include "test_helpers.h"
#include <sys/wait.h>
#include "../src/server/srecord.c"

static int check_recording(int length, size_t available, int expected_status)
{
    FILE *file = tmpfile();
    TEST_CHECK(file != NULL);
    for (int i = 0; i < num_types; i++) {
        uint32_t encoded_length = htonl(i == num_types - 1 ? length : 0);
        TEST_CHECK(fwrite(&encoded_length, sizeof(encoded_length), 1, file) == 1);
    }
    uint32_t value = htonl(42);
    TEST_CHECK(available <= sizeof(value));
    TEST_CHECK(fwrite(&value, 1, available, file) == available);
    rewind(file);

    pid_t child = fork();
    TEST_CHECK(child >= 0);
    if (child == 0) {
        recf1 = file;
        for (int i = 0; i < num_types; i++) {
            bufs[i].start = calloc(1, bufs[i].size);
            if (bufs[i].start == NULL)
                _exit(2);
            *bufs[i].curp = bufs[i].start;
        }
        Get_recording_data();
        if (length == 4 && *playback_opttout != 42)
            _exit(2);
        _exit(0);
    }
    int status;
    TEST_CHECK(waitpid(child, &status, 0) == child);
    TEST_CHECK(fclose(file) == 0);
    TEST_CHECK(WIFEXITED(status));
    TEST_CHECK(WEXITSTATUS(status) == expected_status);
    return 0;
}

int main(void)
{
    TEST_CHECK(check_recording(4, 4, 0) == 0);
    TEST_CHECK(check_recording(4, 2, 1) == 0);
    TEST_CHECK(check_recording(-1, 0, 1) == 0);
    TEST_CHECK(check_recording(3, 3, 1) == 0);
    TEST_CHECK(check_recording(2000, 0, 1) == 0);
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "utf8_files.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/xpilot-paths-XXXXXX";
    assert(mkdtemp(directory));
    char first[256], second[256];
    snprintf(first, sizeof(first), "%s/空白 name.conf", directory);
    snprintf(second, sizeof(second), "%s/改名 map.xp2", directory);
    FILE *file = Xp_fopen(first, "wb");
    assert(file);
    assert(fputs("map: 日本語.xp2\n", file) >= 0 && !fclose(file));
    assert(!Xp_access(first, 4));
    assert(!Xp_rename(first, second));
    int fd = Xp_open_read(second);
    assert(fd >= 0);
    char text[64] = {0};
    assert(read(fd, text, sizeof(text) - 1) == (ssize_t)strlen("map: 日本語.xp2\n"));
    assert(!strcmp(text, "map: 日本語.xp2\n"));
    close(fd);
    assert(!Xp_remove(second));
    assert(!rmdir(directory));
    return 0;
}

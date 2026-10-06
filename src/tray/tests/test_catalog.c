#define _POSIX_C_SOURCE 200809L
#include "map_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    assert(map_catalog_name("日本語 space.xp2"));
    assert(!map_catalog_name("../escape.xp2"));
    assert(!map_catalog_name("a\\escape.xp2"));
    assert(!map_catalog_name("map.xp2.gz"));
    assert(!map_catalog_name("ndh-1.3.xpd"));
    assert(!map_catalog_name("newline\ninjection.xp2"));
    char directory[] = "/tmp/xpilot-maps-XXXXXX";
    assert(mkdtemp(directory));
    const char *names[] = {"z.map", "a.xp", "日本語 space.xp2", "helper.xpd", "Makefile"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    char link[256];
    snprintf(link, sizeof(link), "%s/link.xp2", directory);
    assert(!symlink("a.xp", link));
    map_catalog maps = {0};
    assert(map_catalog_read(directory, &maps));
    assert(maps.count == 3);
    assert(!strcmp(maps.names[0], "a.xp"));
    assert(!strcmp(maps.names[1], "z.map"));
    assert(!strcmp(maps.names[2], "日本語 space.xp2"));
    map_catalog_clear(&maps);
    assert(!maps.names && !maps.count);
    assert(!unlink(link));
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        assert(!unlink(path));
    }
    assert(!rmdir(directory));
    return 0;
}

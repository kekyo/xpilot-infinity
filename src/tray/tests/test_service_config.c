#include "service_config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *map = service_config_map("# comment\nmap: ndh.xp2\nnoQuit: true\n", false);
    assert(map != NULL && strcmp(map, "ndh.xp2") == 0);
    free(map);
    map = service_config_map("map: /shared/space 日本語.xp2\r\n", false);
    assert(map != NULL && strcmp(map, "/shared/space 日本語.xp2") == 0);
    free(map);
    assert(service_config_map("mapData: \\multiline\nmap: ignored.xp2\n", false) == NULL);
    map = service_config_map("XPILOT_SERVER_OPTIONS=\"-noQuit +reportMeta -map ndh.xp2\"\n", true);
    assert(map != NULL && strcmp(map, "ndh.xp2") == 0);
    free(map);
    map = service_config_map("# ignored\nXPILOT_SERVER_OPTIONS='-map \"/shared/a b.xp2\"'\n", true);
    assert(map != NULL && strcmp(map, "/shared/a b.xp2") == 0);
    free(map);
    assert(service_config_map("XPILOT_SERVER_OPTIONS='-map a.xp2 -map b.xp2'", true) == NULL);
    assert(service_config_map("XPILOT_SERVER_OPTIONS='-defaultsFileName other.conf -map a.xp2'", true) == NULL);
    assert(service_config_map("XPILOT_SERVER_OPTIONS='-map \"broken'", true) == NULL);
    const char *path = "/shared/space 日本語 ' \\ \".xp2";
    char *updated = service_config_select_map(
        "# keep this comment\nOTHER=unchanged\n"
        "XPILOT_SERVER_OPTIONS='-noQuit +reportMeta -port 15345 -map old.xp2 +strictMap'\n", path, true);
    assert(updated);
    map = service_config_map(updated, true);
    assert(map && !strcmp(map, path));
    assert(strstr(updated, "# keep this comment\nOTHER=unchanged\n"));
    assert(strstr(updated, "-noQuit +reportMeta -port 15345"));
    assert(strstr(updated, "-strictMap") && !strstr(updated, "+strictMap"));
    free(map); free(updated);
    updated = service_config_select_map("# comment\r\nMAP : old.xp2 # keep\r\nport: 15500\r\n",
                                        "C:/Program Files/日本語/map.xp2", false);
    assert(updated);
    map = service_config_map(updated, false);
    assert(map && !strcmp(map, "C:/Program Files/日本語/map.xp2"));
    assert(strstr(updated, "# keep\r\nport: 15500\r\n"));
    assert(strstr(updated, "strictMap: true\r\n"));
    free(map); free(updated);
    updated = service_config_select_map("XPILOT_SERVER_OPTIONS='-map a.xp2 -mapFileName b.xp2'\n", path, true);
    assert(updated);
    map = service_config_map(updated, true);
    assert(map && !strcmp(map, path));
    free(map); free(updated);
    assert(!service_config_select_map("expand: custom\nmap: old.xp2\n", path, false));
    assert(!service_config_select_map("XPILOT_SERVER_OPTIONS='-defaultsFileName other.conf'", path, true));
    assert(!service_config_select_map("map: old.xp2\n", "/shared/new\ninjected.xp2", false));
    assert(!service_config_select_map("map: old.xp2\n", "../outside.xp2", false));
    assert(service_config_valid("map: 日本語.xp2\r\n", strlen("map: 日本語.xp2\r\n")));
    assert(!service_config_valid("a\0b", 3));
    assert(!service_config_valid("\xef\xbb\xbfmap: x", 9));
    assert(!service_config_valid("\xc0\xaf", 2));
    updated = service_config_select_map("XPILOT_SERVER_OPTIONS='-greeting \"-map\" -map old.xp2'\n", path, true);
    assert(updated && strstr(updated, "-greeting \\\"-map\\\""));
    map = service_config_map(updated, true);
    assert(map && !strcmp(map, path));
    free(map); free(updated);
    assert(!service_config_select_map("XPILOT_SERVER_OPTIONS='-unknown -map old.xp2'", path, true));
    return 0;
}

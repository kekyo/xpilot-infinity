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
    return 0;
}

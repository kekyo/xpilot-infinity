#include "tray_platform.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts(TITLE);
        return 0;
    }
    return tray_platform_run(argc, argv);
}

#include "test_helpers.h"
#include "xpmapedit.h"

xpmap_t map;
char *progname = "test-mapedit-input";
prefs_t prefs[260] = {
    {"mapwidth", "", "Width", 3, MAPWIDTH, map.width_str, NULL, 0, 0, 0, 0},
    {"mapheight", "", "Height", 3, MAPHEIGHT, map.height_str, NULL, 0, 0, 0, 0},
    {"mapname", "", "Name", 254, STRING, map.mapName, NULL, 0, 0, 0, 0},
    {"worldlives", "lives", "Lives", 3, POSINT, map.worldLives, NULL, 0, 0, 0, 0}
};
int numprefs = 4;

Window T_PopupAlert(int type, char *message, char *first, char *second,
                    handler_t first_handler, handler_t second_handler)
{
    (void)type; (void)message; (void)first; (void)second;
    (void)first_handler; (void)second_handler;
    return 0;
}

static int numbers(void)
{
    const char field_name[] = "lives";
    const char field_label[] = "Lives";
    T_FormStringEntry(1, field_name, 0, 0, 50, 20, 0, 0,
                      field_label, map.worldLives, 3, NULL);
    TEST_CHECK(strcmp(T_Form->field->name, field_name) == 0);
    TEST_CHECK(strcmp(T_Form->field->label, field_label) == 0);
    strcpy(T_Form->field->charvar, "42");
    TEST_CHECK(strcmp(map.worldLives, "42") == 0);
    free(T_Form->field->name);
    free(T_Form->field->label);
    T_FormClear(1);
    free(T_Form);
    T_Form = NULL;

    struct { char *input; int length, type; const char *expected; } cases[] = {
        {"1a.2.3", 12, FLOAT, "1.23"},
        {"-.25", 12, FLOAT, "-.25"},
        {"-12", 12, POSINT, "12"},
        {"-1.5", 12, POSFLOAT, "1.5"},
        {"-123456", 4, INT, "-123"},
        {"+123", 12, INT, "123"},
        {"", 4, INT, ""},
        {"123", 0, INT, ""}
    };
    for (size_t i = 0; i < NELEM(cases); i++) {
        char *result = StrToNum(cases[i].input, cases[i].length, cases[i].type);
        TEST_CHECK(result != NULL);
        TEST_CHECK(strcmp(result, cases[i].expected) == 0);
        free(result);
    }

    char name[] = "WoRlDlIvEs";
    TEST_CHECK(AddOption(name, "-1234") == 0);
    TEST_CHECK(strcmp(name, "WoRlDlIvEs") == 0);
    TEST_CHECK(strcmp(map.worldLives, "123") == 0);
    TEST_CHECK(AddOption("mapwidth", "42") == 0);
    TEST_CHECK(AddOption("mapheight", "17") == 0);
    TEST_CHECK(map.width == 42 && map.height == 17);
    char long_name[300];
    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    memset(map.mapName, 'y', sizeof(map.mapName));
    TEST_CHECK(AddOption("mapname", long_name) == 0);
    TEST_CHECK(strlen(map.mapName) == sizeof(map.mapName) - 1);

    char path[] = "/tmp/xpilot-prefs-XXXXXX";
    int fd = mkstemp(path);
    TEST_CHECK(fd >= 0 && close(fd) == 0);
    map.comments = strdup("# user note");
    TEST_CHECK(map.comments != NULL);
    TEST_CHECK(SaveMap(path) == 0);
    free(map.comments);
    map.comments = NULL;
    memset(map.worldLives, 0, sizeof(map.worldLives));
    FILE *file = fopen(path, "r");
    TEST_CHECK(file != NULL);
    while (!feof(file))
        (void)ParseLine(file);
    TEST_CHECK(strcmp(map.worldLives, "123") == 0);
    TEST_CHECK(strstr(map.comments, "# user note") != NULL);
    TEST_CHECK(fclose(file) == 0 && unlink(path) == 0);
    free(map.comments);
    map.comments = NULL;
    return 0;
}

static int maps(void)
{
    struct { const char *contents; int xbm, expected; } cases[] = {
        {"#define m_width 9\n#define m_height 1\n{0x01, 0xFF};\n", 1, 0},
        {"#define m_width 9\n#define m_height 1\n{0x01};\n", 1, 1},
        {"", 1, 1}, {"#define m_width 8\n", 1, 1},
        {"#define m_width 901\n#define m_height 1\n", 1, 1},
        {"#define m_width 8\n#define m_height -1\n", 1, 1},
        {"#define m_width 8\n#define m_height 1\n{0xG0};\n", 1, 1},
        {"#define m_width 8\n#define m_height 1\n{0xFF, 0xFF};\n", 1, 1},
        {"2x1\n6\nName\nAuthor\nxx\n", 0, 0},
        {"2x1\n6\nName\nAuthor\nx", 0, 1},
        {"", 0, 1}, {"2\n", 0, 1}, {"2x1\n", 0, 1},
        {"2x1\n6\n", 0, 1}, {"2x1\n6\nName\n", 0, 1},
        {"901x1\n6\nName\nAuthor\nxx\n", 0, 1},
        {"2x1\n6\nName\nAuthor\n\001x\n", 0, 1}
    };
    for (size_t i = 0; i < NELEM(cases); i++) {
        char path[] = "/tmp/xpilot-mapedit-XXXXXX";
        int fd = mkstemp(path);
        TEST_CHECK(fd >= 0);
        size_t length = strlen(cases[i].contents);
        TEST_CHECK(write(fd, cases[i].contents, length) == (ssize_t)length);
        TEST_CHECK(close(fd) == 0);
        memset(map.data, '?', sizeof(map.data));
        int result = cases[i].xbm ? LoadXbmFile(path) : LoadOldMap(path);
        TEST_CHECK(result == cases[i].expected);
        if (i == 0) {
            TEST_CHECK(map.width == 9 && map.height == 1);
            TEST_CHECK(map.data[0][0] == 'x' && map.data[1][0] == ' ');
            TEST_CHECK(map.data[8][0] == 'x' && map.data[9][0] == '?');
        }
        if (i == 8) {
            TEST_CHECK(map.width == 2 && map.height == 1);
            TEST_CHECK(map.data[0][0] == 'x' && map.data[1][0] == 'x');
            TEST_CHECK(strcmp(map.mapName, "Name") == 0);
            TEST_CHECK(strcmp(map.mapAuthor, "Author") == 0);
            TEST_CHECK(map.edgeWrap == 2);
        }
        TEST_CHECK(unlink(path) == 0);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "maps") == 0)
        return maps();
    TEST_CHECK(numbers() == 0);
    TEST_CHECK(maps() == 0);
    return 0;
}

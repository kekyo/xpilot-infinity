#include "test_helpers.h"
#include "../src/client/mapdata.c"

static int extract_case(const char *name, const char *contents, int expected)
{
    gzFile file = gzopen(name, "wb");
    TEST_CHECK(file != NULL);
    TEST_CHECK(gzwrite(file, contents, strlen(contents)) == (int)strlen(contents));
    TEST_CHECK(gzclose(file) == Z_OK);
    TEST_CHECK(Mapdata_extract(name) == expected);
    TEST_CHECK(unlink(name) == 0);
    return 0;
}

int main(void)
{
    char directory[] = "/tmp/xpilot-mapdata-XXXXXX";
    TEST_CHECK(mkdtemp(directory) != NULL);
    TEST_CHECK(chdir(directory) == 0);
    TEST_CHECK(extract_case("valid.xpd", "XPD 1\ntile 3\nabc", 1) == 0);
    FILE *file = fopen("valid/tile", "rb");
    char contents[4] = {0};
    TEST_CHECK(file != NULL);
    TEST_CHECK(fread(contents, 1, 3, file) == 3);
    TEST_CHECK(strcmp(contents, "abc") == 0);
    TEST_CHECK(fclose(file) == 0);
    TEST_CHECK(unlink("valid/tile") == 0);
    TEST_CHECK(rmdir("valid") == 0);

    TEST_CHECK(extract_case("negative.xpd", "XPD 1\ntile -1\n", 0) == 0);
    TEST_CHECK(extract_case("count.xpd", "XPD -1\n", 0) == 0);
    TEST_CHECK(extract_case("parent.xpd", "XPD 1\n../escape 0\n", 0) == 0);
    TEST_CHECK(extract_case("short.xpd", "XPD 1\ntile 4\nabc", 0) == 0);
    TEST_CHECK(unlink("short/tile") == 0);

    /* Both components fit separately, but the joined path does not. */
    char name[255];
    memset(name, 'x', 249);
    strcpy(name + 249, ".xpd");
    TEST_CHECK(extract_case(name, "XPD 1\nlong-name 0\n", 0) == 0);
    name[249] = '\0';
    TEST_CHECK(rmdir(name) == 0);
    char oversized[600];
    strcpy(oversized, "XPD 1\n");
    memset(oversized + 6, 'x', 580);
    strcpy(oversized + 586, " 0\n");
    TEST_CHECK(extract_case("oversized.xpd", oversized, 0) == 0);

    const char *cases[] = {"negative", "count", "parent", "short", "oversized"};
    for (size_t i = 0; i < NELEM(cases); i++)
        TEST_CHECK(rmdir(cases[i]) == 0);
    TEST_CHECK(chdir("/") == 0);
    TEST_CHECK(rmdir(directory) == 0);
    return 0;
}

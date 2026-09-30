#include "service_config.h"
#include <stdlib.h>
#include <string.h>

static bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *copy(const char *begin, const char *end)
{
    char *value = malloc((size_t)(end - begin) + 1);
    if (value) {
        memcpy(value, begin, (size_t)(end - begin));
        value[end - begin] = '\0';
    }
    return value;
}

/* EnvironmentFile is data, not shell input. Quotes only have special meaning
 * at the start of an assignment value; variable/command expansion is absent. */
static char *environment_options(const char *text)
{
    const char *cursor = text;
    char *options = NULL;
    while (*cursor) {
        while (space(*cursor)) cursor++;
        if (!*cursor) break;
        if (*cursor == '#' || *cursor == ';') {
            while (*cursor && *cursor != '\n') cursor++;
            continue;
        }
        const char *key = cursor;
        while (*cursor && *cursor != '=' && *cursor != '\n') cursor++;
        if (*cursor != '=') continue;
        const char *key_end = cursor++;
        while (key_end > key && space(key_end[-1])) key_end--;
        bool wanted = key_end - key == 21 && !memcmp(key, "XPILOT_SERVER_OPTIONS", 21);
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
        char quote = (*cursor == '\'' || *cursor == '"') ? *cursor++ : 0;
        char *value = malloc(strlen(cursor) + 1);
        if (!value) { free(options); return NULL; }
        size_t used = 0;
        bool closed = !quote;
        while (*cursor) {
            char c = *cursor++;
            if (quote && c == quote) { closed = true; break; }
            if (!quote && c == '\n') break;
            if (c == '\\' && quote != '\'') {
                if (*cursor == '\n') { cursor++; continue; }
                if (!*cursor) { free(value); free(options); return NULL; }
                if (!quote || *cursor == '\\' || *cursor == '"'
                    || *cursor == '$' || *cursor == '`') c = *cursor++;
            }
            value[used++] = c;
        }
        if (quote) {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
            if (!closed || (*cursor && *cursor != '\n')) {
                free(value); free(options); return NULL;
            }
        } else {
            while (used && space(value[used - 1])) used--;
        }
        value[used] = '\0';
        if (wanted) { free(options); options = value; }
        else free(value);
    }
    return options;
}

static char *next_argument(const char **cursor, bool *valid)
{
    while (space(**cursor)) (*cursor)++;
    if (!**cursor) return NULL;
    char *value = malloc(strlen(*cursor) + 1);
    if (!value) { *valid = false; return NULL; }
    size_t used = 0;
    char quote = 0;
    while (**cursor) {
        char c = *(*cursor)++;
        if (!quote && space(c)) break;
        if ((c == '\'' || c == '"') && (!quote || quote == c)) {
            quote = quote ? 0 : c;
            continue;
        }
        if (c == '\\' && quote != '\'') {
            if (!**cursor) { *valid = false; break; }
            c = *(*cursor)++;
        }
        value[used++] = c;
    }
    if (quote) *valid = false;
    value[used] = '\0';
    return value;
}

char *service_config_map(const char *text, bool environment)
{
    char *map = NULL;
    bool valid = true;
    if (environment) {
        char *options = environment_options(text);
        if (!options) return NULL;
        const char *cursor = options;
        char *argument;
        while (valid && (argument = next_argument(&cursor, &valid)) != NULL) {
            if (!strcmp(argument, "-map") || !strcmp(argument, "-mapFileName")) {
                if (map) valid = false;
                else map = next_argument(&cursor, &valid);
                if (!map) valid = false;
            } else if (!strcmp(argument, "-mapData") || !strcmp(argument, "-defaultsFileName")) {
                valid = false;
            }
            free(argument);
        }
        free(options);
    } else {
        const char *cursor = text;
        while (*cursor) {
            const char *end = strchr(cursor, '\n');
            if (!end) end = cursor + strlen(cursor);
            while (cursor < end && space(*cursor)) cursor++;
            const char *colon = memchr(cursor, ':', (size_t)(end - cursor));
            if (*cursor != '#' && colon) {
                const char *key_end = colon;
                while (key_end > cursor && space(key_end[-1])) key_end--;
                char *key = copy(cursor, key_end);
                if (!key) { valid = false; break; }
                if (!strcmp(key, "mapData") || !strcmp(key, "defaultsFileName")) valid = false;
                if (!strcmp(key, "map") || !strcmp(key, "mapFileName")) {
                    if (map) { free(key); valid = false; break; }
                    const char *begin = colon + 1, *value_end = end;
                    while (begin < value_end && space(*begin)) begin++;
                    while (value_end > begin && space(value_end[-1])) value_end--;
                    map = copy(begin, value_end);
                    if (!map) valid = false;
                }
                free(key);
            }
            cursor = *end ? end + 1 : end;
        }
    }
    if (!valid || (map && !*map)) { free(map); map = NULL; }
    return map;
}

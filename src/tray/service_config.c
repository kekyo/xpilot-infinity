#include "service_config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static bool equal(const char *a, const char *b)
{
    while (*a && lower(*a) == lower(*b)) { a++; b++; }
    return !*a && !*b;
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

bool service_config_valid(const char *text, size_t length)
{
    if (length > 1024 * 1024) return false;
    const unsigned char *p = (const unsigned char *)text, *end = p + length;
    while (p < end) {
        uint32_t c = *p++, minimum = 0;
        unsigned extra = 0;
        if (c >= 0xf0 && c <= 0xf4) { extra = 3; c &= 7; minimum = 0x10000; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; c &= 15; minimum = 0x800; }
        else if (c >= 0xc2 && c <= 0xdf) { extra = 1; c &= 31; minimum = 0x80; }
        else if (c >= 0x80) return false;
        while (extra--) {
            if (p == end || (*p & 0xc0) != 0x80) return false;
            c = (c << 6) | (*p++ & 63);
        }
        if (c < minimum || c == 0 || c > 0x10ffff || c == 0xfeff
            || (c >= 0xd800 && c <= 0xdfff) || (c >= 0xfdd0 && c <= 0xfdef)
            || (c & 0xffff) >= 0xfffe) return false;
    }
    return true;
}

/* EnvironmentFile is data, not shell input. Quotes only have special meaning
 * at the start of an assignment value; variable/command expansion is absent. */
static char *environment_options(const char *text, const char **begin,
                                  const char **end, bool *valid)
{
    const char *cursor = text;
    char *options = NULL;
    *begin = *end = text + strlen(text);
    while (*cursor) {
        const char *line = cursor;
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
        if (!value) { *valid = false; break; }
        size_t used = 0;
        bool closed = !quote;
        while (*cursor) {
            char c = *cursor++;
            if (quote && c == quote) { closed = true; break; }
            if (!quote && c == '\n') break;
            if (c == '\\' && quote != '\'') {
                if (*cursor == '\n') { cursor++; continue; }
                if (!*cursor) { *valid = false; break; }
                if (!quote || *cursor == '\\' || *cursor == '"'
                    || *cursor == '$' || *cursor == '`') c = *cursor++;
            }
            value[used++] = c;
        }
        if (quote) {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
            if (!closed || (*cursor && *cursor != '\n')) *valid = false;
            if (*cursor == '\n') cursor++;
        } else {
            while (used && space(value[used - 1])) used--;
        }
        value[used] = '\0';
        if (wanted) {
            free(options); options = value;
            /* Keep the newline preceding this assignment, including blank lines. */
            while (line < key && (*line == '\n' || *line == '\r')) line++;
            *begin = line; *end = cursor;
        } else free(value);
        if (!*valid) break;
    }
    if (!*valid) { free(options); return NULL; }
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

static bool map_key(const char *key)
{
    return equal(key, "map") || equal(key, "mapFileName");
}

static bool indirect_key(const char *key)
{
    return equal(key, "mapData") || equal(key, "defaultsFileName")
        || equal(key, "expand") || equal(key, "define") || equal(key, "region");
}

static bool flag_key(const char *key)
{
    return equal(key, "noQuit") || equal(key, "reportMeta")
        || equal(key, "reportToMetaServer") || equal(key, "strictMap");
}

static void skip_value(const char **cursor, const char *argument, bool *valid)
{
    if ((*argument != '-' && *argument != '+') || !argument[1]) {
        *valid = false;
        return;
    }
    if (flag_key(argument + 1)) return;
    while (space(**cursor)) (*cursor)++;
    /* Without the server's complete option schema, an unknown option followed
     * by another flag is ambiguous. Never consume a possible map as its value. */
    if (!**cursor || **cursor == '-' || **cursor == '+') { *valid = false; return; }
    char *value = next_argument(cursor, valid);
    if (!value) *valid = false;
    free(value);
}

/* Defaults support macros and multiline values. Refuse those forms rather
 * than changing a map that a later expansion could override. */
static bool defaults_line(const char *begin, const char *end, char key[128],
                          const char **value, const char **value_end)
{
    while (begin < end && space(*begin)) begin++;
    *key = '\0';
    *value = *value_end = end;
    if (begin == end || *begin == '#') return true;
    const char *colon = memchr(begin, ':', (size_t)(end - begin));
    if (!colon) return false;
    size_t used = 0;
    while (begin < colon) {
        if (!space(*begin)) {
            if (used == 127) return false;
            key[used++] = *begin;
        }
        begin++;
    }
    key[used] = '\0';
    begin = colon + 1;
    while (begin < end && space(*begin)) begin++;
    const char *comment = begin;
    while (comment < end && *comment != '#') comment++;
    end = comment;
    while (end > begin && space(end[-1])) end--;
    *value = begin; *value_end = end;
    return !indirect_key(key) && (begin == end || *begin != '\\');
}

char *service_config_map(const char *text, bool environment)
{
    if (!service_config_valid(text, strlen(text))) return NULL;
    char *map = NULL;
    bool valid = true;
    if (environment) {
        const char *begin, *end;
        char *options = environment_options(text, &begin, &end, &valid);
        if (!options) return NULL;
        const char *cursor = options;
        char *argument;
        while (valid && (argument = next_argument(&cursor, &valid)) != NULL) {
            if ((*argument == '-' || *argument == '+') && map_key(argument + 1)) {
                if (map) valid = false;
                else map = next_argument(&cursor, &valid);
                if (!map) valid = false;
            } else if ((*argument == '-' || *argument == '+') && indirect_key(argument + 1)) {
                valid = false;
            } else skip_value(&cursor, argument, &valid);
            free(argument);
        }
        free(options);
    } else {
        const char *cursor = text;
        while (*cursor && valid) {
            const char *end = strchr(cursor, '\n');
            if (!end) end = cursor + strlen(cursor);
            char key[128];
            const char *value, *value_end;
            valid = defaults_line(cursor, end, key, &value, &value_end);
            if (valid && map_key(key)) {
                if (map) valid = false;
                else map = copy(value, value_end);
                if (!map) valid = false;
            }
            cursor = *end ? end + 1 : end;
        }
    }
    if (!valid || (map && !*map)) { free(map); map = NULL; }
    return map;
}

static char *select_environment(const char *text, const char *map)
{
    bool valid = true;
    const char *begin, *end;
    char *options = environment_options(text, &begin, &end, &valid);
    if (!valid) return NULL;
    const char *cursor = options ? options : "-noQuit +reportMeta";
    /* Each quoting layer can at most double each byte. */
    size_t capacity = 4 * (strlen(text) + strlen(map)) + 256;
    char *arguments = malloc(capacity), *output = malloc(capacity);
    if (!arguments || !output) { free(options); free(arguments); free(output); return NULL; }
    char *out = arguments;
    char *argument;
    while (valid && *cursor) {
        while (space(*cursor)) cursor++;
        const char *start = cursor;
        argument = next_argument(&cursor, &valid);
        if (!argument) break;
        bool option = *argument == '-' || *argument == '+';
        bool remove = option && (map_key(argument + 1) || equal(argument + 1, "strictMap"));
        if (option && indirect_key(argument + 1)) valid = false;
        if (option && map_key(argument + 1)) {
            char *value = next_argument(&cursor, &valid);
            if (!value) valid = false;
            free(value);
        } else skip_value(&cursor, argument, &valid);
        if (!remove) {
            memcpy(out, start, (size_t)(cursor - start)); out += cursor - start;
            if (out > arguments && !space(out[-1])) *out++ = ' ';
        }
        free(argument);
    }
    const char *suffix = "-strictMap -map \"";
    memcpy(out, suffix, strlen(suffix)); out += strlen(suffix);
    for (const char *p = map; *p; p++) {
        if (*p == '\\' || *p == '"') *out++ = '\\';
        *out++ = *p;
    }
    *out++ = '"'; *out = '\0';
    out = output;
    memcpy(out, text, (size_t)(begin - text)); out += begin - text;
    if (out != output && out[-1] != '\n') *out++ = '\n';
    const char *prefix = "XPILOT_SERVER_OPTIONS=\"";
    memcpy(out, prefix, strlen(prefix)); out += strlen(prefix);
    for (const char *p = arguments; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '$' || *p == '`') *out++ = '\\';
        *out++ = *p;
    }
    *out++ = '"'; *out++ = '\n';
    strcpy(out, end);
    free(arguments); free(options);
    if (!valid) { free(output); return NULL; }
    return output;
}

static char *select_defaults(const char *text, const char *map)
{
    if (strchr(map, '#')) return NULL;
    const char *newline = strstr(text, "\r\n") ? "\r\n" : "\n";
    char *output = malloc(strlen(text) + strlen(map) + 128);
    if (!output) return NULL;
    char *out = output;
    bool wrote_map = false, wrote_strict = false;
    for (const char *p = text; *p;) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *next = *end ? end + 1 : end;
        char key[128];
        const char *value, *value_end;
        if (!defaults_line(p, end, key, &value, &value_end)) { free(output); return NULL; }
        bool is_map = map_key(key), is_strict = equal(key, "strictMap");
        if (is_map || is_strict) {
            bool first = is_map ? !wrote_map : !wrote_strict;
            if (first) {
                const char *label = is_map ? "map: " : "strictMap: ";
                const char *replacement = is_map ? map : "true";
                memcpy(out, label, strlen(label)); out += strlen(label);
                memcpy(out, replacement, strlen(replacement)); out += strlen(replacement);
            }
            if (is_map) wrote_map = true;
            else wrote_strict = true;
            /* Keep inline comments and the original line ending. */
            memcpy(out, value_end, (size_t)(next - value_end)); out += next - value_end;
        } else { memcpy(out, p, (size_t)(next - p)); out += next - p; }
        p = next;
    }
    if (out != output && out[-1] != '\n') {
        memcpy(out, newline, strlen(newline)); out += strlen(newline);
    }
    if (!wrote_map) {
        memcpy(out, "map: ", 5); out += 5;
        memcpy(out, map, strlen(map)); out += strlen(map);
        memcpy(out, newline, strlen(newline)); out += strlen(newline);
    }
    if (!wrote_strict) {
        memcpy(out, "strictMap: true", 15); out += 15;
        memcpy(out, newline, strlen(newline)); out += strlen(newline);
    }
    *out = '\0';
    return output;
}

char *service_config_select_map(const char *text, const char *map, bool environment)
{
    if (!service_config_valid(text, strlen(text)) || !service_config_valid(map, strlen(map))) return NULL;
    size_t length = strlen(map);
    if (!length || (map[0] != '/' && !(length >= 3 && map[1] == ':'
        && (map[2] == '/' || map[2] == '\\')))) return NULL;
    for (const char *p = map; *p; p++) if ((unsigned char)*p < 32) return NULL;
    return environment ? select_environment(text, map) : select_defaults(text, map);
}

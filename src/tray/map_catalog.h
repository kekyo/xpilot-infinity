#ifndef XPILOT_MAP_CATALOG_H
#define XPILOT_MAP_CATALOG_H
#include <stdbool.h>
#include <stddef.h>

/** Installed noncompressed maps, sorted by UTF-8 filename. */
typedef struct {
    char **names; /**< Owned UTF-8 filenames without directory components. */
    size_t count; /**< Number of selectable regular files. */
} map_catalog;

/** Read an installed map directory without following symbolic links.
 * @param directory Absolute UTF-8 directory path.
 * @param catalog Output initialized even on failure; release with map_catalog_clear().
 * @return true on complete enumeration; false on I/O, encoding or allocation failure.
 */
bool map_catalog_read(const char *directory, map_catalog *catalog);
/** Release an enumeration and reset it to empty.
 * @param catalog Enumeration to clear.
 */
void map_catalog_clear(map_catalog *catalog);
/** Check that an ID names a supported map within one directory.
 * @param name UTF-8 filename, without separators, controls or dot components.
 * @return true for a noncompressed .xp2, .xp or .map filename.
 */
bool map_catalog_name(const char *name);
#endif

#ifndef XPILOT_SERVICE_CONFIG_H
#define XPILOT_SERVICE_CONFIG_H
#include <stdbool.h>
#include <stddef.h>

/** Extract an unambiguous configured map without executing the configuration.
 * @param text NUL-terminated UTF-8 service configuration.
 * @param environment true for a systemd EnvironmentFile, false for server defaults.
 * @return Owned map value to free(), or NULL if absent, ambiguous or unsupported.
 */
char *service_config_map(const char *text, bool environment);

/** Replace map options and enable strictMap, preserving unrelated settings.
 * @param text Current NUL-terminated UTF-8 configuration.
 * @param map Absolute path already validated against the shared map catalog.
 * @param environment true for systemd, false for server defaults.
 * @return Owned replacement to free(), or NULL for unsupported/invalid input.
 */
char *service_config_select_map(const char *text, const char *map, bool environment);

/** Validate bounded UTF-8 text accepted by the shared configuration interface.
 * @param text Bytes to validate; embedded NUL and BOM are not accepted.
 * @param length Byte count, excluding any terminating NUL; maximum 1 MiB.
 * @return true if valid; CRLF and LF line endings are accepted.
 */
bool service_config_valid(const char *text, size_t length);
#endif

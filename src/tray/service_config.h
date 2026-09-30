#ifndef XPILOT_SERVICE_CONFIG_H
#define XPILOT_SERVICE_CONFIG_H
#include <stdbool.h>

/** Extract an unambiguous configured map without executing the configuration.
 * @param text NUL-terminated UTF-8 service configuration.
 * @param environment true for a systemd EnvironmentFile, false for server defaults.
 * @return Owned map value to free(), or NULL if absent, ambiguous or unsupported.
 */
char *service_config_map(const char *text, bool environment);
#endif

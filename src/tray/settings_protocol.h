#ifndef XPILOT_SETTINGS_PROTOCOL_H
#define XPILOT_SETTINGS_PROTOCOL_H
/** System-bus name and interface of the fixed-service configuration helper. */
#define XP_SETTINGS_BUS "org.xpilot.Infinity.ServerSettings1"
/** Configuration helper object path. */
#define XP_SETTINGS_PATH "/org/xpilot/Infinity/ServerSettings"
/** Dedicated map-selection authorization, separate from free-text editing. */
#define XP_SETTINGS_MAP_ACTION "org.xpilot.infinity.select-map"
/** Free-text editing authorization, independent of catalog map selection. */
#define XP_SETTINGS_EDIT_ACTION "org.xpilot.infinity.apply-configuration"
/** Fixed Linux service configuration; never supplied by a requesting process. */
#define XP_SETTINGS_FILE "/etc/default/xpilot-infinity-server"
/** Last helper operation, keyed by configuration generation. */
#define XP_SETTINGS_RESULT "/etc/default/.xpilot-infinity-settings-result"
#endif

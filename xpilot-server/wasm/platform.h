/* C99/POSIX facilities supplied by the fixed Node.js Emscripten target. */
#ifndef XP_NODE_PLATFORM_H
#define XP_NODE_PLATFORM_H
#include "../../xpilot-browser/wasm/platform.h"
#undef CONF_DATADIR
#define CONF_DATADIR "/.xpilot-wasm/assets/"
#define CONF_LOGFILE "/.xpilot-wasm/data/log.txt"
#define SELECT_SCHED 1
#define HAVE_LIBEXPAT 1
#define HAVE_ZLIB_H 1
#define PACKAGE_NAME "xpilot-infinity"
#define WSLAY_VERSION "1.1.1"
#endif

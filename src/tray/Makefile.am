SUBDIRS = . tests
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror
noinst_LIBRARIES = libxptray.a
libxptray_a_SOURCES = tray_controller.c tray_controller.h \
    service_control.h service_native.h
if COND_WINDOWS
libxptray_a_SOURCES += service_win32.c
else
AM_CPPFLAGS = $(TRAY_CFLAGS) -DGIO_VERSION_MIN_REQUIRED=GLIB_VERSION_2_60 \
    -DGIO_VERSION_MAX_ALLOWED=GLIB_VERSION_2_60
libxptray_a_SOURCES += service_systemd.c
endif

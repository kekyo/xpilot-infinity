SUBDIRS = . tests
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror
noinst_LIBRARIES = libxptray.a
libxptray_a_SOURCES = tray_controller.c tray_controller.h \
    service_control.h service_native.h tray_menu.c tray_menu.h \
    service_config.c service_config.h
bin_PROGRAMS = xpilot-infinity-tray
xpilot_infinity_tray_SOURCES = main.c tray_platform.h
xpilot_infinity_tray_LDADD = libxptray.a
AM_CPPFLAGS = -I$(top_builddir)/src/common
main.$(OBJEXT): $(top_builddir)/src/common/version.h
$(top_builddir)/src/common/version.h:
	$(MAKE) -C $(top_builddir)/src/common version.h
if COND_WINDOWS
libxptray_a_SOURCES += service_win32.c service_paths_win32.c service_paths_win32.h
xpilot_infinity_tray_SOURCES += tray_win32.c
xpilot_infinity_tray_LDADD += tray-res.o -ladvapi32 -luser32 -lshell32 -lgdi32
xpilot_infinity_tray_LDFLAGS = -mwindows
bin_PROGRAMS += xpilot-infinity-service-helper
xpilot_infinity_service_helper_SOURCES = service_helper_win32.c service_paths_win32.c
xpilot_infinity_service_helper_LDADD = -ladvapi32 -lshell32
xpilot_infinity_service_helper_LDFLAGS = -municode -mwindows
WINDRES = @WINDRES@
tray-res.o: $(srcdir)/tray.rc $(top_srcdir)/images/icon.ico
	$(WINDRES) -I$(top_srcdir) $(srcdir)/tray.rc $@
CLEANFILES = tray-res.o
else
AM_CPPFLAGS += $(TRAY_CFLAGS) -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_60 \
    -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_60 \
    -DGTK_VERSION_MIN_REQUIRED=GTK_VERSION_3_22 -DGTK_VERSION_MAX_ALLOWED=GTK_VERSION_3_22 \
    -DXPILOT_TRAY_ICON=\"$(pkgdatadir)/icon-1254.png\"
libxptray_a_SOURCES += service_systemd.c tray_sni.c tray_sni.h
xpilot_infinity_tray_SOURCES += tray_linux.c
xpilot_infinity_tray_LDADD += $(TRAY_LIBS)
dist_pkgdata_DATA = $(top_srcdir)/images/icon-1254.png
endif
EXTRA_DIST = tray.rc

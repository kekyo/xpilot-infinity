SUBDIRS = . tests
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror
noinst_LIBRARIES = libxptray.a
libxptray_a_SOURCES = tray_controller.c tray_controller.h \
    service_control.h service_native.h tray_menu.c tray_menu.h \
    service_config.c service_config.h map_catalog.c map_catalog.h settings_protocol.h \
    config_editor.c config_editor.h
bin_PROGRAMS = xpilot-infinity-tray
xpilot_infinity_tray_SOURCES = main.c tray_platform.h
xpilot_infinity_tray_LDADD = libxptray.a
AM_CPPFLAGS = -I$(top_builddir)/src/common -I$(top_srcdir)/src/common
main.$(OBJEXT): $(top_builddir)/src/common/version.h
$(top_builddir)/src/common/version.h:
	$(MAKE) -C $(top_builddir)/src/common version.h
if COND_WINDOWS
libxptray_a_SOURCES += service_win32.c service_paths_win32.c service_paths_win32.h \
    settings_file_win32.c settings_file_win32.h settings_transfer_win32.c settings_transfer_win32.h \
    $(top_srcdir)/src/common/utf8_files.c
xpilot_infinity_tray_SOURCES += tray_win32.c
xpilot_infinity_tray_LDADD += tray-res.o -ladvapi32 -luser32 -lshell32 -lgdi32 -lole32 -luuid -lbcrypt
xpilot_infinity_tray_LDFLAGS = -mwindows
bin_PROGRAMS += xpilot-infinity-service-helper
xpilot_infinity_service_helper_SOURCES = service_helper_win32.c
xpilot_infinity_service_helper_LDADD = libxptray.a -ladvapi32 -lshell32 -luser32 -lole32 -luuid -lbcrypt
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
AM_CPPFLAGS += -DXPILOT_MAP_DIRECTORY=\"$(pkgdatadir)/maps\"
libxptray_a_SOURCES += service_systemd.c tray_sni.c tray_sni.h tray_xembed.c tray_xembed.h
xpilot_infinity_tray_SOURCES += tray_linux.c
xpilot_infinity_tray_LDADD += $(TRAY_LIBS)
dist_pkgdata_DATA = $(top_srcdir)/images/icon-1254.png
libexec_PROGRAMS = xpilot-infinity-settings-helper
xpilot_infinity_settings_helper_SOURCES = service_helper_linux.c settings_file_linux.c settings_file_linux.h
xpilot_infinity_settings_helper_CPPFLAGS = $(AM_CPPFLAGS) $(TRAY_POLKIT_CFLAGS)
xpilot_infinity_settings_helper_LDADD = libxptray.a $(TRAY_POLKIT_LIBS)
polkitactionsdir = $(datadir)/polkit-1/actions
dist_polkitactions_DATA = data/org.xpilot.infinity.policy
dbusconfigdir = $(sysconfdir)/dbus-1/system.d
dist_dbusconfig_DATA = data/org.xpilot.Infinity.ServerSettings1.conf
dbusservicesdir = $(datadir)/dbus-1/system-services
dbusservices_DATA = org.xpilot.Infinity.ServerSettings1.service
FUNCITY = npm exec --yes --package=funcity-cli@1.5.0 -- funcity
org.xpilot.Infinity.ServerSettings1.service: $(srcdir)/data/org.xpilot.Infinity.ServerSettings1.service.fc Makefile
	$(FUNCITY) --no-rc run -D 'libexecdir=$(libexecdir)' -i $< > $@.tmp
	mv $@.tmp $@
CLEANFILES = org.xpilot.Infinity.ServerSettings1.service org.xpilot.Infinity.ServerSettings1.service.tmp
endif
EXTRA_DIST = tray.rc data/org.xpilot.Infinity.ServerSettings1.service.fc

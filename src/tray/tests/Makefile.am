AM_CPPFLAGS = -I$(srcdir)/..
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror
check_PROGRAMS = test-controller test-menu test-service-config
noinst_PROGRAMS = service-driver
test_controller_SOURCES = test_controller.c ../tray_controller.c
test_menu_SOURCES = test_menu.c ../tray_menu.c
test_service_config_SOURCES = test_service_config.c ../service_config.c
service_driver_SOURCES = service_driver.c
service_driver_LDADD = ../libxptray.a
if COND_WINDOWS
service_driver_LDADD += -ladvapi32 -luser32 -lshell32 -lole32 -luuid
else
AM_CPPFLAGS += $(TRAY_CFLAGS)
service_driver_LDADD += $(TRAY_LIBS)
check_PROGRAMS += test-sni
check_PROGRAMS += test-sni-hosts
test_sni_hosts_SOURCES = test_sni_hosts.c
test_sni_hosts_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_TEST_ICON=\"$(abs_top_srcdir)/images/icon-1254.png\"
test_sni_hosts_LDADD = ../libxptray.a $(TRAY_LIBS)
check_PROGRAMS += test-xembed
test_xembed_SOURCES = test_xembed.c
test_xembed_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_TEST_ICON=\"$(abs_top_srcdir)/images/icon-1254.png\"
test_xembed_LDADD = ../libxptray.a $(TRAY_LIBS) -lXtst
LOG_COMPILER = $(SHELL) $(srcdir)/run-test.sh
check_PROGRAMS += test-catalog test-settings-file test-editor
test_editor_SOURCES = test_editor.c ../config_editor.c ../service_config.c
check_PROGRAMS += test-utf8-files
test_utf8_files_SOURCES = test_utf8_files.c $(top_srcdir)/src/common/utf8_files.c
test_utf8_files_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/common
test_catalog_SOURCES = test_catalog.c ../map_catalog.c ../service_config.c
test_settings_file_SOURCES = test_settings_file.c ../settings_file_linux.c ../service_config.c
test_settings_file_LDADD = $(TRAY_POLKIT_LIBS)
noinst_PROGRAMS += test-desktop test-auth-cancel
test_auth_cancel_SOURCES = test_auth_cancel.c
test_auth_cancel_CPPFLAGS = $(AM_CPPFLAGS) $(TRAY_POLKIT_CFLAGS)
test_auth_cancel_LDADD = $(TRAY_POLKIT_LIBS)
test_desktop_SOURCES = test_desktop.c
test_desktop_CPPFLAGS = $(AM_CPPFLAGS) $(TRAY_TEST_CFLAGS)
test_desktop_LDADD = $(TRAY_LIBS) -lXtst $(TRAY_TEST_LIBS)
test_sni_SOURCES = test_sni.c
test_sni_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_TEST_ICON=\"$(abs_top_srcdir)/images/icon-1254.png\"
test_sni_LDADD = ../libxptray.a $(TRAY_LIBS)
TESTS = test-controller test-menu test-service-config test-sni test-catalog test-settings-file test-utf8-files test-editor test-xembed
TESTS += test-sni-hosts

.PHONY: integration-check
XPILOT_CONTACT_TARGET_PROBE = $(abs_top_builddir)/tests/test-contact-target-probe
integration-check: service-driver test-desktop test-auth-cancel
	XPILOT_SERVICE_DRIVER='$(abs_builddir)/service-driver' \
	XPILOT_SERVER_BINARY='$(abs_top_builddir)/src/server/xpilot-infinity-server' \
	XPILOT_CONTACT_TARGET_PROBE='$(XPILOT_CONTACT_TARGET_PROBE)' \
	XPILOT_TEST_SOURCE_DIR='$(abs_top_srcdir)' \
	XPILOT_TEST_PKGDATADIR='$(pkgdatadir)' \
	XPILOT_TRAY_BINARY='$(abs_top_builddir)/src/tray/xpilot-infinity-tray' \
	XPILOT_DESKTOP_TEST='$(abs_builddir)/test-desktop' \
	XPILOT_AUTH_CANCEL_TEST='$(abs_builddir)/test-auth-cancel' \
	XPILOT_SETTINGS_HELPER='$(abs_top_builddir)/src/tray/xpilot-infinity-settings-helper' \
	$(SHELL) $(srcdir)/run-systemd.sh
endif
EXTRA_DIST = run-systemd.sh run-systemd-inside.sh xpilot-tray-test.service Containerfile test-strict-map.sh test-settings-helper.sh test-settings-editor.sh test-default-editor.sh
EXTRA_DIST += run-test.sh record-xembed.sh
CLEANFILES = test-xembed.mkv

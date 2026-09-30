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
service_driver_LDADD += -ladvapi32 -luser32 -lshell32
else
AM_CPPFLAGS += $(TRAY_CFLAGS)
service_driver_LDADD += $(TRAY_LIBS)
check_PROGRAMS += test-sni
noinst_PROGRAMS += test-desktop
test_desktop_SOURCES = test_desktop.c
test_desktop_LDADD = $(TRAY_LIBS)
test_sni_SOURCES = test_sni.c
test_sni_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_TEST_ICON=\"$(abs_top_srcdir)/images/icon-1254.png\"
test_sni_LDADD = ../libxptray.a $(TRAY_LIBS)
TESTS = test-controller test-menu test-service-config test-sni

.PHONY: integration-check
integration-check: service-driver test-desktop
	XPILOT_SERVICE_DRIVER='$(abs_builddir)/service-driver' \
	XPILOT_SERVER_BINARY='$(abs_top_builddir)/src/server/xpilot-infinity-server' \
	XPILOT_CONTACT_TARGET_PROBE='$(abs_top_builddir)/tests/test-contact-target-probe' \
	XPILOT_TEST_SOURCE_DIR='$(abs_top_srcdir)' \
	XPILOT_TEST_PKGDATADIR='$(pkgdatadir)' \
	XPILOT_TRAY_BINARY='$(abs_top_builddir)/src/tray/xpilot-infinity-tray' \
	XPILOT_DESKTOP_TEST='$(abs_builddir)/test-desktop' \
	$(SHELL) $(srcdir)/run-systemd.sh
endif
EXTRA_DIST = run-systemd.sh run-systemd-inside.sh xpilot-tray-test.service Containerfile

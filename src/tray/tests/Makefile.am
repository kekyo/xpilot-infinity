AM_CPPFLAGS = -I$(srcdir)/..
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror
check_PROGRAMS = test-controller
noinst_PROGRAMS = service-driver
test_controller_SOURCES = test_controller.c ../tray_controller.c
service_driver_SOURCES = service_driver.c
service_driver_LDADD = ../libxptray.a
if COND_WINDOWS
service_driver_LDADD += -ladvapi32 -luser32
else
AM_CPPFLAGS += $(TRAY_CFLAGS)
service_driver_LDADD += $(TRAY_LIBS)
TESTS = test-controller

.PHONY: integration-check
integration-check: service-driver
	XPILOT_SERVICE_DRIVER='$(abs_builddir)/service-driver' \
	XPILOT_SERVER_BINARY='$(abs_top_builddir)/src/server/xpilot-infinity-server' \
	XPILOT_CONTACT_TARGET_PROBE='$(abs_top_builddir)/tests/test-contact-target-probe' \
	XPILOT_TEST_SOURCE_DIR='$(abs_top_srcdir)' \
	XPILOT_TEST_PKGDATADIR='$(pkgdatadir)' \
	$(SHELL) $(srcdir)/run-systemd.sh
endif
EXTRA_DIST = run-systemd.sh run-systemd-inside.sh xpilot-tray-test.service

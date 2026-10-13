
bin_PROGRAMS = xpilot-infinity-replay
AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-I$(top_builddir)/src/common -I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/client
xpilot_infinity_replay_SOURCES = buttons.c buttons.h ppm_image.c ppm_image.h \
	xp-replay.c xp-replay.h
xpilot_infinity_replay_LDADD = $(top_builddir)/src/common/libxpcommon.a @X_LIBS@ @X_PRE_LIBS@ @X_EXTRA_LIBS@ -lX11
SUBDIRS = tools

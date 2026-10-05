AUTOMAKE_OPTIONS = subdir-objects

SCREW_UP = screw-up

AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-I$(top_srcdir)/vendor/wslay/lib/includes \
	-I$(top_srcdir)/vendor/wslay/lib \
	-I$(top_srcdir)/vendor/mtwister \
	-DWSLAY_VERSION=\"1.1.1\"

if COND_WINDOWS
AM_CPPFLAGS += -DHAVE_WINSOCK2_H
endif

noinst_LIBRARIES = libxpcommon.a
BUILT_SOURCES = version.h
CLEANFILES = version.h build-metadata.txt

.PHONY: build-metadata-force

build-metadata-force:

build-metadata.txt: build-metadata-force \
		$(top_srcdir)/config/resolve-build-metadata.sh
	@resolved_metadata=`XPILOT_VERSION='$(XPILOT_VERSION)' \
		XPILOT_COMMIT_ID='$(XPILOT_COMMIT_ID)' \
		SCREW_UP='$(SCREW_UP)' \
		$(top_srcdir)/config/resolve-build-metadata.sh` || exit $$?; \
	if test ! -f $@ \
		|| test "x$$resolved_metadata" != "x`cat $@`"; then \
		printf '%s\n' "$$resolved_metadata" > $@.tmp; \
		mv -f $@.tmp $@; \
	fi

version.h: version.h.template build-metadata.txt
	@resolved_version=`sed -n '1p' build-metadata.txt`; \
	resolved_commit=`sed -n '2p' build-metadata.txt`; \
	sed -e "s/__XPILOT_BUILD_VERSION__/$$resolved_version/g" \
		-e "s/__XPILOT_BUILD_COMMIT__/$$resolved_commit/g" \
		version.h.template > $@.tmp; \
	if test -f $@ && cmp -s $@.tmp $@; then \
		rm -f $@.tmp; \
	else \
		mv -f $@.tmp $@; \
	fi

libxpcommon_a_SOURCES = \
	arraylist.c arraylist.h astershape.h audio.h \
	bit.h \
	checknames.c checknames.h click.h commonproto.h config.c const.h \
	utf8_files.c utf8_files.h \
	error.c error.h \
	game_transport.c game_transport.h \
	item.h \
	keys.h \
	list.c list.h \
	math.c metaserver.h \
	net.c net.h record_drain.c record_drain.h \
	recording_input.c recording_input.h \
	record_session.c record_session.h record_transport.c record_transport.h \
	pack.h packet.h portability.c portability.h \
	randommt.c \
	$(top_srcdir)/vendor/mtwister/mtwister.c \
	$(top_srcdir)/vendor/mtwister/mtwister.h \
	rules.h \
	secure_random.c secure_random.h \
	session_protocol.c session_protocol.h \
	session_token.c session_token.h \
	utf8_names.c utf8_names.h \
	websocket_http.c websocket_http.h \
	websocket_transport.c websocket_transport.h \
	setup.h shipshape.c shipshape.h socklib.c socklib.h \
	strcasecmp.c strdup.c strlcpy.c \
	$(top_srcdir)/vendor/wslay/lib/wslay_event.c \
	$(top_srcdir)/vendor/wslay/lib/wslay_event.h \
	$(top_srcdir)/vendor/wslay/lib/wslay_frame.c \
	$(top_srcdir)/vendor/wslay/lib/wslay_frame.h \
	$(top_srcdir)/vendor/wslay/lib/wslay_net.c \
	$(top_srcdir)/vendor/wslay/lib/wslay_net.h \
	$(top_srcdir)/vendor/wslay/lib/wslay_queue.c \
	$(top_srcdir)/vendor/wslay/lib/wslay_queue.h \
	$(top_srcdir)/vendor/wslay/lib/includes/wslay/wslay.h \
	types.h win32.h \
	version.h \
	wreckshape.h \
	xpcommon.h xpconfig.h xpmap.h xpmemory.c

EXTRA_DIST = $(top_srcdir)/vendor/wslay/COPYING \
	$(top_srcdir)/vendor/mtwister/README.md

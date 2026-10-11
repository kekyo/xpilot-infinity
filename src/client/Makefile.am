
if COND_MINGW_VENDORED
-include $(top_builddir)/mingw-deps.mk
XP_TEXT_SDL3_CFLAGS = $(MINGW_SDL3_CFLAGS)
XP_TEXT_SDL3_TTF_CFLAGS = $(MINGW_SDL3_TTF_CFLAGS)
XP_SOUND_CFLAGS = $(MINGW_SOUND_CFLAGS)
else
XP_TEXT_SDL3_CFLAGS = @SDL3_CFLAGS@
XP_TEXT_SDL3_TTF_CFLAGS = @SDL3_TTF_CFLAGS@
XP_SOUND_CFLAGS = @SOUND_CFLAGS@
endif

AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-I$(top_builddir)/src/common -I$(top_srcdir)/src/common \
	$(XP_TEXT_SDL3_CFLAGS) $(XP_TEXT_SDL3_TTF_CFLAGS) \
	$(XP_SOUND_CFLAGS) @NATIVE_TEXT_CFLAGS@

noinst_LIBRARIES = libxpclient.a
libxpclient_a_SOURCES = \
	client.c client.h \
	client_session.c client_session.h \
	clientcommand.c clientcommand.h clientrank.c clientrank.h \
	configure.h connectparam.h connect_target.c connect_target.h \
	control_session.c control_session.h \
	datagram.c datagram.h default.c \
	event.c \
	gfx2d.c gfx2d.h \
	guimap.h guiobjects.h \
	mapdata.c messages.c meta.c meta.h \
	netclient.c netclient.h \
	paint.c paint.h paintmap.c paintobjects.c \
	option.c option.h \
	query.c \
	recordfmt.h transport_display.c transport_display.h \
	session_connection.c session_connection.h \
	session_transport.c session_transport.h \
	talk.h talkmacros.c textinterface.c \
	micro_delay.c micro_delay.h micro_delay_test_support.h \
	native_text.c native_text.h native_text_internal.h \
	text_config.c text_config.h text_utf8.c \
	xpclient.h

if COND_SOUND
libxpclient_a_SOURCES += caudio.c caudio.h oalaudio.c
endif

# From section Conditional subdirectories with AM_CONDITIONAL
# in GNU automake manual.
if COND_SDL_CLIENT
  MAYBE_SDL = sdl
else
  MAYBE_SDL =
endif

if COND_X11_CLIENT
  MAYBE_X11 = x11
else
  MAYBE_X11 =
endif

# kps - . in SUBDIRS is needed to make sure libxpclient.a is compiled before
# SDL client
SUBDIRS = . items $(MAYBE_X11) $(MAYBE_SDL)

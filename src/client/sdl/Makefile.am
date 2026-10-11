
AUTOMAKE_OPTIONS = subdir-objects

if COND_MINGW_VENDORED
-include $(top_builddir)/mingw-deps.mk
XP_SDL3_CFLAGS = $(MINGW_SDL3_CFLAGS)
XP_SDL3_LIBS = $(MINGW_SDL3_LIBS)
XP_SDL3_IMAGE_CFLAGS = $(MINGW_SDL3_IMAGE_CFLAGS)
XP_SDL3_IMAGE_LIBS = $(MINGW_SDL3_IMAGE_LIBS)
XP_SDL3_TTF_CFLAGS = $(MINGW_SDL3_TTF_CFLAGS)
XP_SDL3_TTF_LIBS = $(MINGW_SDL3_TTF_LIBS)
XP_SOUND_LIBS = $(MINGW_SOUND_LIBS)
else
XP_SDL3_CFLAGS = @SDL3_CFLAGS@
XP_SDL3_LIBS = @SDL3_LIBS@
XP_SDL3_IMAGE_CFLAGS = @SDL3_IMAGE_CFLAGS@
XP_SDL3_IMAGE_LIBS = @SDL3_IMAGE_LIBS@
XP_SDL3_TTF_CFLAGS = @SDL3_TTF_CFLAGS@
XP_SDL3_TTF_LIBS = @SDL3_TTF_LIBS@
XP_SOUND_LIBS = @SOUND_LIBS@
endif

bin_PROGRAMS = xpilot-infinity-sdl
noinst_LIBRARIES = libxp_libtess2.a
AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-DSDL_DISABLE_OLD_NAMES \
	-I$(top_builddir)/src/common -I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/vendor/libtess2/Include \
	$(XP_SDL3_CFLAGS) $(XP_SDL3_IMAGE_CFLAGS) $(XP_SDL3_TTF_CFLAGS) \
	@NATIVE_TEXT_CFLAGS@ @GL_CFLAGS@
libxp_libtess2_a_SOURCES = \
	$(top_srcdir)/vendor/libtess2/Include/tesselator.h \
	$(top_srcdir)/vendor/libtess2/Source/bucketalloc.c \
	$(top_srcdir)/vendor/libtess2/Source/bucketalloc.h \
	$(top_srcdir)/vendor/libtess2/Source/dict.c \
	$(top_srcdir)/vendor/libtess2/Source/dict.h \
	$(top_srcdir)/vendor/libtess2/Source/geom.c \
	$(top_srcdir)/vendor/libtess2/Source/geom.h \
	$(top_srcdir)/vendor/libtess2/Source/mesh.c \
	$(top_srcdir)/vendor/libtess2/Source/mesh.h \
	$(top_srcdir)/vendor/libtess2/Source/priorityq.c \
	$(top_srcdir)/vendor/libtess2/Source/priorityq.h \
	$(top_srcdir)/vendor/libtess2/Source/sweep.c \
	$(top_srcdir)/vendor/libtess2/Source/sweep.h \
	$(top_srcdir)/vendor/libtess2/Source/tess.c \
	$(top_srcdir)/vendor/libtess2/Source/tess.h
xpilot_infinity_sdl_SOURCES = \
	asteroid_data.h DT_drawtext.c DT_drawtext.h \
	SDL_console.c SDL_console.h console.c console.h glwidgets.c glwidgets.h \
	gameinput.c gameinput.h gameloop.c gameloop.h \
	gl_diagnostics.c gl_diagnostics.h \
	image_geometry.c image_geometry.h images.c images.h images_test_support.h \
	main.c radar.c radar.h \
	paint_transform.c paint_transform.h \
	polygon_geometry.c polygon_geometry.h \
	renderer.c renderer.h renderer_backend.h \
	renderer_gl.h renderer_gl_core.c renderer_gl_core.h \
	renderer_gl_es.h \
	score_font.c score_font.h \
	sdlclipboard.c sdlclipboard.h \
	sdlcompat.c sdlcompat.h \
	sdlevent.c sdlevent.h sdlgui.c sdlinit.c sdlkeys.c sdlkeys.h \
	sdlgameframe.c sdlgameframe.h \
	sdlmetaframe.c sdlmetaframe.h sdlrenderer.c sdlrenderer.h \
	sdl_server_browser.c sdl_server_browser.h \
	sdluiimage.c sdluiimage.h \
	sdlpaint.c surface_primitives.c surface_primitives.h \
	surface_rgba8.c surface_rgba8.h \
	sdlpaint.h sdlwindow.c sdlwindow.h text.c text.h \
	text_atlas.c text_atlas.h text_geometry.c text_geometry.h \
	text_renderer.c text_renderer.h \
	unicode_text_renderer.c unicode_text_renderer.h \
	text_ttf_atlas.c text_ttf_atlas.h todo.c \
	sdlmeta.h sdlmeta.c sdlinit.h xpclient_sdl.h
xpilot_infinity_sdl_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a \
	libxp_libtess2.a \
	$(XP_SDL3_IMAGE_LIBS) $(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS) \
	@NATIVE_TEXT_LIBS@ @GL_LIBS@ $(XP_SOUND_LIBS) @W32_LIBS@

if COND_WINDOWS
  xpilot_infinity_sdl_SOURCES += win32hacks.c
endif

EXTRA_DIST = $(top_srcdir)/vendor/libtess2/LICENSE.txt \
	$(top_srcdir)/vendor/libtess2/README.md

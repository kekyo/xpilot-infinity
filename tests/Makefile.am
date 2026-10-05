if COND_MINGW_VENDORED
-include $(top_builddir)/mingw-deps.mk
XP_SDL3_CFLAGS = $(MINGW_SDL3_CFLAGS)
XP_SDL3_LIBS = $(MINGW_SDL3_LIBS)
XP_SDL3_IMAGE_CFLAGS = $(MINGW_SDL3_IMAGE_CFLAGS)
XP_SDL3_IMAGE_LIBS = $(MINGW_SDL3_IMAGE_LIBS)
XP_SDL3_TTF_CFLAGS = $(MINGW_SDL3_TTF_CFLAGS)
XP_SDL3_TTF_LIBS = $(MINGW_SDL3_TTF_LIBS)
else
XP_SDL3_CFLAGS = @SDL3_CFLAGS@
XP_SDL3_LIBS = @SDL3_LIBS@
XP_SDL3_IMAGE_CFLAGS = @SDL3_IMAGE_CFLAGS@
XP_SDL3_IMAGE_LIBS = @SDL3_IMAGE_LIBS@
XP_SDL3_TTF_CFLAGS = @SDL3_TTF_CFLAGS@
XP_SDL3_TTF_LIBS = @SDL3_TTF_LIBS@
endif

AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-DSDL_DISABLE_OLD_NAMES \
	-I$(top_builddir)/src/common -I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/sdl \
	-I$(top_srcdir)/vendor/libtess2/Include \
	$(XP_SDL3_CFLAGS) $(XP_SDL3_IMAGE_CFLAGS) $(XP_SDL3_TTF_CFLAGS) \
	@NATIVE_TEXT_CFLAGS@ @GL_CFLAGS@

if COND_WINDOWS
check_PROGRAMS = test-framed-stream test-websocket-transport \
	test-game-transport test-checknames \
	test-connect-target test-contact-target-probe \
	test-transport-display test-session-token test-socket-io \
	test-native-socket-handle test-recording-input test-session-flow \
	test-randommt test-ppm-image test-micro-delay \
	test-sdl-server-browser \
	test-sdl-versions test-score-font test-native-text test-game-input
TESTS =
else
check_PROGRAMS = test-framed-stream test-websocket-transport \
	test-game-transport test-checknames \
	test-connect-target test-contact-target-probe \
	test-transport-display test-session-token test-socket-io \
	test-native-socket-handle \
	test-recording-input \
	test-session-flow \
	test-meta-environment test-randommt test-ppm-image test-micro-delay \
	test-sdl-server-browser \
	test-sdl-versions test-score-font test-native-text \
	test-game-input test-gameloop test-sdl-keys test-sdl-console \
	test-sdl-surface \
	test-surface-rgba8 test-surface-primitives test-sdl-window \
	test-sdl-clipboard test-sdl-event test-gl-core-resources \
	test-renderer-commands test-renderer-context-recovery \
	test-renderer-geometry \
	test-renderer-gl-core test-paint-transform test-image-geometry \
	test-polygon-geometry \
	test-gfx2d-picture test-images test-radar-renderer test-sdl-ui-image \
	test-sdl-meta-frame test-sdl-game-frame test-sdl-renderer-scissor \
	test-sdl-init-core-profile test-sdl-renderer-core-context \
	test-sdlpaint-stage test-paintmap-vdecor \
	test-glwidgets-scissor test-glwidgets-primitives \
	test-sdlmeta-primitives test-sdlgui-appearing \
	test-sdlgui-hud-primitives test-sdlgui-asteroids \
	test-sdlgui-polygons test-sdlgui-sparks test-sdlgui-ships \
	test-text-geometry test-text-atlas test-unicode-text-renderer \
	test-text-ttf-atlas test-text-renderer test-text-cache-adapter
TESTS = test-framed-stream test-websocket-transport \
	test-game-transport test-checknames test-connect-target \
	test-transport-display test-session-token \
	test-socket-io \
	test-native-socket-handle \
	test-recording-input \
	test-session-flow \
	test-meta-environment test-randommt test-ppm-image test-micro-delay \
	test-sdl-server-browser \
	test-sdl-versions test-score-font test-native-text \
	test-game-input test-gameloop test-sdl-keys test-sdl-console \
	test-sdl-surface \
	test-surface-rgba8 test-surface-primitives test-sdl-window \
	test-sdl-clipboard test-sdl-event test-renderer-commands \
	test-renderer-context-recovery test-renderer-geometry \
	test-paint-transform run-gl-core-resources.sh \
	test-image-geometry test-polygon-geometry test-gfx2d-picture \
	test-images test-radar-renderer test-sdl-ui-image test-sdl-meta-frame \
	test-sdl-game-frame \
	test-sdl-init-core-profile test-sdlpaint-stage \
	run-sdl-core-boundary.sh \
	test-paintmap-vdecor \
	test-sdl-renderer-scissor test-glwidgets-scissor \
	test-glwidgets-primitives test-sdlmeta-primitives \
	test-sdlgui-appearing test-sdlgui-hud-primitives \
	test-sdlgui-asteroids test-sdlgui-polygons test-sdlgui-sparks \
	test-sdlgui-ships \
	test-text-geometry \
	test-text-atlas test-unicode-text-renderer \
	test-text-ttf-atlas test-text-renderer \
	test-text-cache-adapter \
	run-renderer-gl-core.sh run-sdl-renderer-core-context.sh \
	run-build-metadata.sh run-build-wrapper.sh \
	run-build-package.sh run-package-tools.sh run-package-windows.sh \
	run-windows-archive.sh run-macos-info-plist.sh \
	run-source-encoding.sh \
	run-c23-server-build.sh run-vendored-zlib.sh \
	run-container-publish.sh run-container-image.sh run-quadlet.sh run-e2e.sh
if COND_SOUND
check_PROGRAMS += test-audio-device
TESTS += test-audio-device
endif
if COND_X11_CLIENT
check_PROGRAMS += test-x11-text
TESTS += run-x11-text.sh
endif
endif

test_game_transport_SOURCES = test_game_transport.c
test_game_transport_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_checknames_SOURCES = test_checknames.c
test_checknames_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_randommt_SOURCES = test_randommt.c
test_randommt_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_ppm_image_SOURCES = test_ppm_image.c \
	$(top_srcdir)/src/replay/ppm_image.c
test_ppm_image_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/replay

test_micro_delay_SOURCES = test_micro_delay.c \
	$(top_srcdir)/src/client/micro_delay.c
test_micro_delay_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_MICRO_DELAY_TEST_HOOKS
test_micro_delay_LDADD = @W32_LIBS@

test_session_token_SOURCES = test_session_token.c
test_session_token_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_connect_target_SOURCES = test_connect_target.c \
	$(top_srcdir)/src/client/connect_target.c
test_connect_target_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_contact_target_probe_SOURCES = test_contact_target_probe.c \
	$(top_srcdir)/src/client/sdl/win32hacks.c
test_contact_target_probe_LDADD = \
	$(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a @W32_LIBS@

test_transport_display_SOURCES = test_transport_display.c \
	$(top_srcdir)/src/client/transport_display.c

test_framed_stream_SOURCES = test_framed_stream.c
test_framed_stream_LDADD = $(top_builddir)/src/common/libxpcommon.a \
	@W32_LIBS@

test_websocket_transport_SOURCES = test_websocket_transport.c
test_websocket_transport_LDADD = \
	$(top_builddir)/src/common/libxpcommon.a @W32_LIBS@

test_socket_io_SOURCES = test_socket_io.c
test_socket_io_LDADD = $(top_builddir)/src/common/libxpcommon.a \
	@W32_LIBS@

test_native_socket_handle_SOURCES = test_native_socket_handle.c

test_recording_input_SOURCES = test_recording_input.c
test_recording_input_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_session_flow_SOURCES = test_session_flow.c \
	$(top_srcdir)/src/server/session_acceptor.c
test_session_flow_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/server
test_session_flow_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a @W32_LIBS@

test_meta_environment_SOURCES = test_meta_environment.c \
	$(top_srcdir)/src/client/meta.c \
	$(top_srcdir)/src/client/meta_test_support.h \
	$(top_srcdir)/src/client/connect_target.c \
	$(top_srcdir)/src/client/transport_display.c \
	$(top_srcdir)/src/common/game_transport.c \
	$(top_srcdir)/src/common/list.c \
	$(top_srcdir)/src/common/strdup.c \
	$(top_srcdir)/src/common/strlcpy.c
test_meta_environment_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_META_TEST_HOOKS
test_meta_environment_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_meta_environment_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections

test_sdl_server_browser_SOURCES = test_sdl_server_browser.c \
	$(top_srcdir)/src/client/sdl/sdl_server_browser.c
test_sdl_server_browser_LDADD = \
	$(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a

test_sdl_versions_SOURCES = test_sdl_versions.c
test_sdl_versions_LDADD = $(XP_SDL3_IMAGE_LIBS) $(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS)

test_score_font_SOURCES = test_score_font.c \
	$(top_srcdir)/src/client/sdl/score_font.c
test_score_font_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_TEST_FONT_DIR=\"$(top_srcdir)/lib/fonts\"
test_score_font_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS) @NATIVE_TEXT_LIBS@

test_native_text_SOURCES = test_native_text.c
test_native_text_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_TEST_FONT_DIR=\"$(top_srcdir)/lib/fonts\"
test_native_text_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a \
	$(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS) @NATIVE_TEXT_LIBS@

test_game_input_SOURCES = test_game_input.c \
	$(top_srcdir)/src/client/sdl/gameinput.c
test_game_input_LDADD = $(XP_SDL3_LIBS)

test_gameloop_SOURCES = test_gameloop.c \
	$(top_srcdir)/src/client/sdl/gameloop.c
test_gameloop_LDADD = $(XP_SDL3_LIBS)

test_sdl_keys_SOURCES = test_sdl_keys.c \
	$(top_srcdir)/src/client/sdl/sdlkeys.c
test_sdl_keys_LDADD = $(XP_SDL3_LIBS)

test_sdl_console_SOURCES = test_sdl_console.c \
	$(top_srcdir)/src/client/sdl/SDL_console.c
test_sdl_console_LDADD = $(XP_SDL3_IMAGE_LIBS) $(XP_SDL3_LIBS)

test_sdl_surface_SOURCES = test_sdl_surface.c \
	$(top_srcdir)/src/client/sdl/sdlcompat.c
test_sdl_surface_LDADD = $(XP_SDL3_LIBS)

test_surface_rgba8_SOURCES = test_surface_rgba8.c \
	$(top_srcdir)/src/client/sdl/surface_rgba8.c \
	$(top_srcdir)/src/client/sdl/sdlcompat.c
test_surface_rgba8_LDADD = $(XP_SDL3_LIBS)

test_surface_primitives_SOURCES = test_surface_primitives.c \
	$(top_srcdir)/src/client/sdl/surface_primitives.c
test_surface_primitives_LDADD = $(XP_SDL3_LIBS)

test_sdl_window_SOURCES = test_sdl_window.c \
	$(top_srcdir)/src/client/sdl/sdlwindow.c
test_sdl_window_LDADD = $(XP_SDL3_LIBS)

test_sdl_clipboard_SOURCES = test_sdl_clipboard.c \
	$(top_srcdir)/src/client/sdl/sdlclipboard.c
test_sdl_clipboard_LDADD = $(XP_SDL3_LIBS)

test_sdl_event_SOURCES = test_sdl_event.c \
	$(top_srcdir)/src/client/sdl/sdlevent.c
test_sdl_event_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,--wrap=SDL_GetMouseState
test_sdl_event_LDADD = $(XP_SDL3_LIBS)

test_gl_core_resources_SOURCES = test_gl_core_resources.c \
	$(top_srcdir)/src/client/sdl/gl_diagnostics.c \
	$(top_srcdir)/src/client/sdl/renderer.c \
	$(top_srcdir)/src/client/sdl/renderer_gl_core.c \
	$(top_srcdir)/src/client/sdl/text.c \
	$(top_srcdir)/src/client/sdl/text_atlas.c \
	$(top_srcdir)/src/client/sdl/text_atlas.h \
	$(top_srcdir)/src/client/sdl/text_geometry.c \
	$(top_srcdir)/src/client/sdl/text_geometry.h \
	$(top_srcdir)/src/client/sdl/text_ttf_atlas.c \
	$(top_srcdir)/src/client/sdl/text_ttf_atlas.h
test_gl_core_resources_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_TEST_FONT_PATH=\"$(top_srcdir)/lib/fonts/FreeSansBoldOblique.ttf\"
test_gl_core_resources_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_gl_core_resources_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=TTF_OpenFont \
	-Wl,--wrap=TTF_CloseFont
test_gl_core_resources_LDADD = $(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS) \
	@GL_LIBS@

test_renderer_commands_SOURCES = test_renderer_commands.c \
	$(top_srcdir)/src/client/sdl/renderer.c
test_renderer_commands_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,--wrap=malloc -Wl,--wrap=calloc

test_renderer_context_recovery_SOURCES = \
	test_renderer_context_recovery.c \
	$(top_srcdir)/src/client/sdl/renderer.c

test_renderer_geometry_SOURCES = test_renderer_geometry.c \
	$(top_srcdir)/src/client/sdl/renderer.c

test_renderer_gl_core_SOURCES = test_renderer_gl_core.c \
	$(top_srcdir)/src/client/sdl/renderer.c \
	$(top_srcdir)/src/client/sdl/renderer_gl_core.c \
	$(top_srcdir)/src/client/sdl/text_geometry.c
test_renderer_gl_core_LDADD = $(XP_SDL3_LIBS) @GL_LIBS@

test_paint_transform_SOURCES = test_paint_transform.c \
	$(top_srcdir)/src/client/sdl/paint_transform.c

test_image_geometry_SOURCES = test_image_geometry.c \
	$(top_srcdir)/src/client/sdl/image_geometry.c

test_polygon_geometry_SOURCES = test_polygon_geometry.c \
	$(top_srcdir)/src/client/sdl/polygon_geometry.c \
	$(top_srcdir)/src/client/sdl/polygon_geometry.h
test_polygon_geometry_LDADD = \
	$(top_builddir)/src/client/sdl/libxp_libtess2.a

test_gfx2d_picture_SOURCES = test_gfx2d_picture.c \
	$(top_srcdir)/src/client/gfx2d.c
test_gfx2d_picture_LDFLAGS = $(AM_LDFLAGS) -Wl,--wrap=malloc
test_gfx2d_picture_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_images_SOURCES = test_images.c \
	$(top_srcdir)/src/client/sdl/images.c \
	$(top_srcdir)/src/client/sdl/image_geometry.c \
	$(top_srcdir)/src/common/strdup.c
test_images_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_IMAGE_TEST_HOOKS
test_images_LDADD = $(XP_SDL3_LIBS)

test_radar_renderer_SOURCES = test_radar_renderer.c \
	$(top_srcdir)/src/client/sdl/radar.c
test_radar_renderer_CPPFLAGS = $(AM_CPPFLAGS) -DXPILOT_RADAR_TEST_HOOKS
test_radar_renderer_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_radar_renderer_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_radar_renderer_LDADD = $(XP_SDL3_LIBS)

test_sdl_ui_image_SOURCES = test_sdl_ui_image.c \
	$(top_srcdir)/src/client/sdl/sdluiimage.c \
	$(top_srcdir)/src/client/sdl/sdlmetaframe.c \
	$(top_srcdir)/src/client/sdl/glwidgets.c \
	$(top_srcdir)/src/client/sdl/surface_rgba8.c \
	$(top_srcdir)/src/client/sdl/sdlcompat.c
test_sdl_ui_image_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdl_ui_image_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_sdl_ui_image_LDADD = $(XP_SDL3_LIBS)

test_sdl_meta_frame_SOURCES = test_sdl_meta_frame.c \
	$(top_srcdir)/src/client/sdl/sdlmetaframe.c

test_sdl_game_frame_SOURCES = test_sdl_game_frame.c \
	$(top_srcdir)/src/client/sdl/sdlgameframe.c

test_sdlpaint_stage_SOURCES = test_sdlpaint_stage.c \
	$(top_srcdir)/src/client/sdl/sdlpaint.c
test_sdlpaint_stage_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_SDLPAINT_TEST_HOOKS
test_sdlpaint_stage_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlpaint_stage_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_sdlpaint_stage_LDADD = $(XP_SDL3_LIBS)

test_paintmap_vdecor_SOURCES = test_paintmap_vdecor.c \
	$(top_srcdir)/src/client/paintmap.c
test_paintmap_vdecor_CPPFLAGS = $(AM_CPPFLAGS) -DPAINT_FREE=0
test_paintmap_vdecor_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_paintmap_vdecor_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections

test_sdl_renderer_scissor_SOURCES = test_sdl_renderer_scissor.c \
	$(top_srcdir)/src/client/sdl/renderer.c \
	$(top_srcdir)/src/client/sdl/sdlrenderer.c
test_sdl_renderer_scissor_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdl_renderer_scissor_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=SDL_GL_GetCurrentWindow \
	-Wl,--wrap=SDL_GL_GetCurrentContext \
	-Wl,--wrap=SDL_GL_GetProcAddress \
	-Wl,--wrap=SDL_GetWindowSizeInPixels
test_sdl_renderer_scissor_LDADD = $(XP_SDL3_LIBS) @GL_LIBS@

test_sdl_init_core_profile_SOURCES = test_sdl_init_core_profile.c \
	$(top_srcdir)/src/client/sdl/sdlinit.c
test_sdl_init_core_profile_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdl_init_core_profile_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,--gc-sections -Wl,--wrap=SDL_Init -Wl,--wrap=SDL_Quit \
	-Wl,--wrap=TTF_Init -Wl,--wrap=TTF_Quit \
	-Wl,--wrap=SDL_GL_SetAttribute -Wl,--wrap=SDL_CreateWindow
test_sdl_init_core_profile_LDADD = $(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS)

test_sdl_renderer_core_context_SOURCES = \
	test_sdl_renderer_core_context.c \
	$(top_srcdir)/src/client/sdl/renderer.c \
	$(top_srcdir)/src/client/sdl/renderer_gl_core.c \
	$(top_srcdir)/src/client/sdl/sdlrenderer.c
test_sdl_renderer_core_context_LDADD = $(XP_SDL3_LIBS) @GL_LIBS@

test_glwidgets_scissor_SOURCES = test_glwidgets_scissor.c \
	$(top_srcdir)/src/client/sdl/glwidgets.c
test_glwidgets_scissor_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_glwidgets_scissor_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_glwidgets_scissor_LDADD = $(XP_SDL3_LIBS)

test_glwidgets_primitives_SOURCES = test_glwidgets_primitives.c \
	$(top_srcdir)/src/client/sdl/glwidgets.c
test_glwidgets_primitives_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_GLWIDGETS_TEST_HOOKS
test_glwidgets_primitives_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_glwidgets_primitives_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_glwidgets_primitives_LDADD = $(XP_SDL3_LIBS)

test_sdlmeta_primitives_SOURCES = test_sdlmeta_primitives.c \
	$(top_srcdir)/src/client/sdl/sdlmeta.c
test_sdlmeta_primitives_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_SDLMETA_TEST_HOOKS
test_sdlmeta_primitives_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlmeta_primitives_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_sdlmeta_primitives_LDADD = $(XP_SDL3_LIBS)

test_sdlgui_appearing_SOURCES = test_sdlgui_appearing.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c \
	$(top_srcdir)/src/client/sdl/appearing_batch_test_support.h \
	$(top_srcdir)/src/client/paintobjects.c
test_sdlgui_appearing_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_APPEARING_BATCH_TEST_HOOKS
test_sdlgui_appearing_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_appearing_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=realloc \
	-Wl,--wrap=Gui_paint_paused \
	-Wl,--wrap=Gui_paint_ecm \
	-Wl,--wrap=Gui_paint_ship \
	-Wl,--wrap=Gui_paint_all_connectors_begin \
	-Wl,--wrap=Gui_paint_refuel \
	-Wl,--wrap=Gui_paint_connector \
	-Wl,--wrap=Gui_paint_transporter
test_sdlgui_appearing_LDADD = $(XP_SDL3_LIBS)

test_sdlgui_hud_primitives_SOURCES = test_sdlgui_hud_primitives.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c
test_sdlgui_hud_primitives_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_SDLGUI_TEST_HOOKS
test_sdlgui_hud_primitives_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_hud_primitives_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_sdlgui_hud_primitives_LDADD = $(XP_SDL3_LIBS)

test_sdlgui_asteroids_SOURCES = test_sdlgui_asteroids.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c \
	$(top_srcdir)/src/client/sdl/asteroid_batch_test_support.h \
	$(top_srcdir)/src/client/sdl/polygon_geometry.c \
	$(top_srcdir)/src/client/sdl/polygon_geometry.h \
	$(top_srcdir)/src/client/paintobjects.c
test_sdlgui_asteroids_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_ASTEROID_BATCH_TEST_HOOKS
test_sdlgui_asteroids_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_asteroids_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=realloc -Wl,--wrap=free \
	-Wl,--wrap=Gui_paint_item_object \
	-Wl,--wrap=Gui_paint_ball \
	-Wl,--wrap=Gui_paint_ball_connector \
	-Wl,--wrap=Gui_paint_mine \
	-Wl,--wrap=Gui_paint_sparks_begin \
	-Wl,--wrap=Gui_paint_spark \
	-Wl,--wrap=Gui_paint_sparks_end \
	-Wl,--wrap=Gui_paint_wreck \
	-Wl,--wrap=Gui_paint_setup_worm \
	-Wl,--wrap=Gui_paint_fastshot \
	-Wl,--wrap=Gui_paint_teamshot \
	-Wl,--wrap=Gui_paint_missiles_begin \
	-Wl,--wrap=Gui_paint_missiles_end \
	-Wl,--wrap=Gui_paint_missile \
	-Wl,--wrap=Gui_paint_lasers_begin \
	-Wl,--wrap=Gui_paint_lasers_end \
	-Wl,--wrap=Gui_paint_laser
test_sdlgui_asteroids_LDADD = \
	$(top_builddir)/src/client/sdl/libxp_libtess2.a $(XP_SDL3_LIBS)

test_sdlgui_polygons_SOURCES = test_sdlgui_polygons.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c \
	$(top_srcdir)/src/client/sdl/polygon_cache_test_support.h \
	$(top_srcdir)/src/client/sdl/polygon_geometry.c \
	$(top_srcdir)/src/client/sdl/polygon_geometry.h
test_sdlgui_polygons_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_POLYGON_CACHE_TEST_HOOKS
test_sdlgui_polygons_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_polygons_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
test_sdlgui_polygons_LDADD = \
	$(top_builddir)/src/client/sdl/libxp_libtess2.a $(XP_SDL3_LIBS)

test_sdlgui_sparks_SOURCES = test_sdlgui_sparks.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c \
	$(top_srcdir)/src/client/sdl/spark_batch_test_support.h \
	$(top_srcdir)/src/client/paintobjects.c
test_sdlgui_sparks_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_SPARK_BATCH_TEST_HOOKS
test_sdlgui_sparks_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_sparks_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=realloc \
	-Wl,--wrap=Gui_paint_item_object \
	-Wl,--wrap=Gui_paint_ball \
	-Wl,--wrap=Gui_paint_ball_connector \
	-Wl,--wrap=Gui_paint_mine \
	-Wl,--wrap=Gui_paint_asteroids_begin \
	-Wl,--wrap=Gui_paint_asteroids_end \
	-Wl,--wrap=Gui_paint_asteroid \
	-Wl,--wrap=Gui_paint_setup_worm \
	-Wl,--wrap=Gui_paint_fastshot \
	-Wl,--wrap=Gui_paint_teamshot \
	-Wl,--wrap=Gui_paint_missiles_begin \
	-Wl,--wrap=Gui_paint_missiles_end \
	-Wl,--wrap=Gui_paint_missile \
	-Wl,--wrap=Gui_paint_lasers_begin \
	-Wl,--wrap=Gui_paint_lasers_end \
	-Wl,--wrap=Gui_paint_laser
test_sdlgui_sparks_LDADD = $(XP_SDL3_LIBS)

test_sdlgui_ships_SOURCES = test_sdlgui_ships.c \
	$(top_srcdir)/src/client/sdl/sdlgui.c \
	$(top_srcdir)/src/client/paintobjects.c
test_sdlgui_ships_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_sdlgui_ships_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=Gui_paint_paused \
	-Wl,--wrap=Gui_paint_appearing_begin \
	-Wl,--wrap=Gui_paint_appearing \
	-Wl,--wrap=Gui_paint_appearing_end \
	-Wl,--wrap=Gui_paint_ecm \
	-Wl,--wrap=Gui_paint_all_connectors_begin \
	-Wl,--wrap=Gui_paint_refuel \
	-Wl,--wrap=Gui_paint_connector \
	-Wl,--wrap=Gui_paint_transporter
test_sdlgui_ships_LDADD = $(XP_SDL3_LIBS)

test_text_geometry_SOURCES = test_text_geometry.c \
	$(top_srcdir)/src/client/sdl/text_geometry.c \
	$(top_srcdir)/src/client/sdl/text_geometry.h

test_text_atlas_SOURCES = test_text_atlas.c \
	$(top_srcdir)/src/client/sdl/text_atlas.c \
	$(top_srcdir)/src/client/sdl/text_atlas.h
test_text_atlas_LDADD = $(XP_SDL3_LIBS)

test_text_ttf_atlas_SOURCES = test_text_ttf_atlas.c \
	$(top_srcdir)/src/client/sdl/text_ttf_atlas.c \
	$(top_srcdir)/src/client/sdl/text_ttf_atlas.h \
	$(top_srcdir)/src/client/sdl/text_atlas.c \
	$(top_srcdir)/src/client/sdl/text_atlas.h \
	$(top_srcdir)/src/client/sdl/text_geometry.c \
	$(top_srcdir)/src/client/sdl/text_geometry.h
test_text_ttf_atlas_CPPFLAGS = $(AM_CPPFLAGS) \
	-DXPILOT_TEST_FONT_PATH=\"$(top_srcdir)/lib/fonts/FreeSansBoldOblique.ttf\"
test_text_ttf_atlas_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,--wrap=TTF_GetGlyphMetrics
test_text_ttf_atlas_LDADD = $(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS)

test_text_renderer_SOURCES = test_text_renderer.c \
	$(top_srcdir)/src/client/sdl/text_renderer.c \
	$(top_srcdir)/src/client/sdl/text_renderer.h \
	$(top_srcdir)/src/client/sdl/text_geometry.c \
	$(top_srcdir)/src/client/sdl/text_geometry.h
test_text_renderer_LDFLAGS = $(AM_LDFLAGS) -Wl,--wrap=malloc

test_unicode_text_renderer_SOURCES = test_unicode_text_renderer.c \
	$(top_srcdir)/src/client/sdl/unicode_text_renderer.c \
	$(top_srcdir)/src/client/sdl/unicode_text_renderer.h

test_x11_text_SOURCES = test_x11_text.c \
	$(top_srcdir)/src/client/x11/x11_text.c \
	$(top_srcdir)/src/client/x11/x11_text.h
test_x11_text_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/client/x11 \
	-DXPILOT_TEST_FONT_DIR=\"$(top_srcdir)/lib/fonts\"
test_x11_text_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a \
	$(XP_SDL3_TTF_LIBS) $(XP_SDL3_LIBS) @NATIVE_TEXT_LIBS@ \
	@XRENDER_LIBS@ -lX11

test_text_cache_adapter_SOURCES = test_text_cache_adapter.c \
	$(top_srcdir)/src/client/sdl/text.c
test_text_cache_adapter_CFLAGS = $(AM_CFLAGS) \
	-ffunction-sections -fdata-sections
test_text_cache_adapter_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections
test_text_cache_adapter_LDADD = $(top_builddir)/src/common/libxpcommon.a

test_audio_device_SOURCES = test_audio_device.c
test_audio_device_CFLAGS = $(AM_CFLAGS) -ffunction-sections -fdata-sections
test_audio_device_LDFLAGS = $(AM_LDFLAGS) -Wl,--gc-sections \
	-Wl,--wrap=alutInit -Wl,--wrap=alutGetError \
	-Wl,--wrap=alutGetErrorString -Wl,--wrap=alutExit \
	-Wl,--wrap=alListenerf -Wl,--wrap=alDopplerFactor \
	-Wl,--wrap=alSpeedOfSound -Wl,--wrap=alGetError \
	-Wl,--wrap=alGenSources -Wl,--wrap=alDeleteSources
test_audio_device_LDADD = $(top_builddir)/src/client/libxpclient.a \
	$(top_builddir)/src/common/libxpcommon.a @SOUND_LIBS@

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)
AM_TESTS_ENVIRONMENT = \
	XPILOT_TEST_PREFIX='$(prefix)'; export XPILOT_TEST_PREFIX; \
	XPILOT_TEST_BINDIR='$(bindir)'; export XPILOT_TEST_BINDIR; \
	XPILOT_TEST_PKGDATADIR='$(pkgdatadir)'; export XPILOT_TEST_PKGDATADIR; \
	XPILOT_CONTACT_TARGET_PROBE='$(abs_builddir)/test-contact-target-probe'; \
	export XPILOT_CONTACT_TARGET_PROBE; \
	XPILOT_GL_CORE_RESOURCE_TEST_BINARY='$(abs_builddir)/test-gl-core-resources'; \
	export XPILOT_GL_CORE_RESOURCE_TEST_BINARY; \
	XPILOT_RENDERER_GL_CORE_TEST_BINARY='$(abs_builddir)/test-renderer-gl-core'; \
	export XPILOT_RENDERER_GL_CORE_TEST_BINARY; \
	XPILOT_SDL_CORE_CONTEXT_TEST_BINARY='$(abs_builddir)/test-sdl-renderer-core-context'; \
	export XPILOT_SDL_CORE_CONTEXT_TEST_BINARY; \
	XPILOT_X11_TEXT_TEST_BINARY='$(abs_builddir)/test-x11-text'; \
	export XPILOT_X11_TEXT_TEST_BINARY; \
	XPILOT_SDL_OBJECT_DIR='$(abs_top_builddir)/src/client/sdl'; \
	export XPILOT_SDL_OBJECT_DIR; \
	XPILOT_SDL_BINARY='$(abs_top_builddir)/src/client/sdl/xpilot-infinity-sdl'; \
	export XPILOT_SDL_BINARY; \
	XPILOT_BUILD_WRAPPER='$(abs_top_srcdir)/build.sh'; \
	export XPILOT_BUILD_WRAPPER; \
	XPILOT_BUILD_METADATA_RESOLVER='$(abs_top_srcdir)/config/resolve-build-metadata.sh'; \
	export XPILOT_BUILD_METADATA_RESOLVER; \
	XPILOT_BUILD_PACKAGE='$(abs_top_srcdir)/build_package.sh'; \
	export XPILOT_BUILD_PACKAGE; \
	XPILOT_CONTAINER_BUILDER='$(abs_top_srcdir)/build_container_image.sh'; \
	export XPILOT_CONTAINER_BUILDER; \
	XPILOT_CONTAINER_PUBLISHER='$(abs_top_srcdir)/build_container_image_all.sh'; \
	export XPILOT_CONTAINER_PUBLISHER; \
	XPILOT_CONTAINER_SOURCE_DIR='$(abs_top_srcdir)'; \
	export XPILOT_CONTAINER_SOURCE_DIR; \
	XPILOT_QUADLET_DIR='$(abs_top_srcdir)/containers'; \
	export XPILOT_QUADLET_DIR; \
	XPILOT_WINDOWS_ARCHIVER='$(abs_top_srcdir)/config/package-windows.mjs'; \
	export XPILOT_WINDOWS_ARCHIVER; \
	XPILOT_WINDOWS_INSTALLER_BUILDER='$(abs_top_srcdir)/config/build-windows-installer.sh'; \
	export XPILOT_WINDOWS_INSTALLER_BUILDER; \
	XPILOT_WINDOWS_INSTALLER_SCRIPT='$(abs_top_srcdir)/config/xpilot-infinity.nsi'; \
	export XPILOT_WINDOWS_INSTALLER_SCRIPT; \
	XPILOT_WINDOWS_SERVER_CONFIG='$(abs_top_srcdir)/config/xpilot-infinity-server.conf'; \
	export XPILOT_WINDOWS_SERVER_CONFIG; \
	XPILOT_WINDOWS_ICON='$(abs_top_srcdir)/images/icon.ico'; \
	export XPILOT_WINDOWS_ICON; \
	XPILOT_WINDOWS_PACKAGER='$(abs_top_srcdir)/config/package-windows.sh'; \
	export XPILOT_WINDOWS_PACKAGER; \
	XPILOT_MACOS_INFO_PLIST='$(abs_top_srcdir)/contrib/macosx/English.lproj/InfoPlist.strings'; \
	export XPILOT_MACOS_INFO_PLIST; \
	XPILOT_SOURCE_ENCODING_ROOT='$(abs_top_srcdir)'; \
	export XPILOT_SOURCE_ENCODING_ROOT; \
	XPILOT_C23_CC='$(CC)'; export XPILOT_C23_CC; \
	XPILOT_C23_SOURCE_DIR='$(abs_top_srcdir)'; export XPILOT_C23_SOURCE_DIR; \
	XPILOT_C23_BUILD_DIR='$(abs_top_builddir)'; export XPILOT_C23_BUILD_DIR; \
	XPILOT_VENDOR_ZLIB_SOURCE='$(abs_top_srcdir)/vendor/zlib'; \
	export XPILOT_VENDOR_ZLIB_SOURCE;

EXTRA_DIST = run-build-metadata.sh run-build-wrapper.sh \
	run-e2e.sh run-full-suite.sh \
	tcp-reconnect-proxy.mjs session-reconnect-proxy.mjs \
	run-gl-core-resources.sh \
	run-renderer-gl-core.sh run-sdl-core-boundary.sh \
	run-sdl-renderer-core-context.sh run-c23-server-build.sh \
	run-vendored-zlib.sh \
	run-wine-suite.sh run-build-package.sh run-package-tools.sh run-package-windows.sh \
	run-windows-archive.sh run-macos-info-plist.sh \
	run-container-publish.sh run-container-image.sh run-quadlet.sh \
	run-source-encoding.sh test-source-encoding.mjs \
	test-windows-archive.mjs test-macos-info-plist.mjs test_helpers.h

run-sdl-core-boundary.log: \
	$(top_builddir)/src/client/sdl/xpilot-infinity-sdl

.PHONY: e2e-install
run-e2e.log: e2e-install
e2e-install:
	@case '$(prefix)' in \
		/tmp/*|/var/tmp/*) $(MAKE) -C '$(abs_top_builddir)' install ;; \
		*) : ;; \
	esac

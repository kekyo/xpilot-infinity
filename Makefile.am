if COND_MINGW_VENDORED
SUBDIRS = vendor/mingw src lib doc contrib
else
SUBDIRS = src lib doc contrib
endif

if COND_SDL_CLIENT
SUBDIRS += tests
endif
dist_pkgdata_DATA = mapconvert.py
EXTRA_DIST = Dockerfile .dockerignore build.sh build_container_image.sh \
	build_container_image_all.sh \
	build_package.sh build_package_all.sh build_package_npm.sh prereq.sh \
	containers/xpilot-infinity-server.container \
	containers/xpilot-infinity-server-data.volume \
	scripts/build_linux_dist_container.sh config/package-windows.sh \
	config/package-windows.mjs config/build-windows-installer.sh \
	config/xpilot-infinity.nsi config/xpilot-infinity-server.conf \
	config/resolve-build-metadata.sh images/icon-1254.png images/icon.ico \
	debian/changelog.in debian/copyright \
	debian/xpilot-infinity-server.default \
	debian/xpilot-infinity-server.service \
	debian/xpilot-infinity.postinst debian/xpilot-infinity.postrm \
	debian/xpilot-infinity.prerm debian/xpilot-infinity.desktop

if COND_WINDOWS
MINGW_TEST_JOBS =
MAKENSIS = makensis

.PHONY: windows-package windows-installer windows-check mingw-deps-clean
windows-package: all
	$(SHELL) $(top_srcdir)/config/package-windows.sh \
		--source-dir '$(abs_top_srcdir)' \
		--build-dir '$(abs_top_builddir)' \
		--dependency-prefix '@MINGW_DEPS_PREFIX@' \
		--output '@MINGW_PACKAGE_DIR@'

windows-installer: windows-package
	MAKENSIS='$(MAKENSIS)' \
	$(SHELL) $(top_srcdir)/config/build-windows-installer.sh \
		--package-dir '@MINGW_PACKAGE_DIR@' \
		--output '$(abs_top_builddir)/xpilot-infinity-setup.exe' \
		--version "`sed -n '1p' '$(abs_top_builddir)/src/common/build-metadata.txt'`" \
		--arch '@MINGW_ARCH@' \
		--icon '$(abs_top_srcdir)/images/icon.ico'

windows-check: windows-installer
	$(SHELL) $(top_srcdir)/tests/run-wine-suite.sh \
		--build-dir '$(abs_top_builddir)' \
		--package-dir '@MINGW_PACKAGE_DIR@' \
		--installer '$(abs_top_builddir)/xpilot-infinity-setup.exe' \
		--wine-prefix '@MINGW_WINE_PREFIX@' \
		--arch '@MINGW_ARCH@' \
		--jobs '$(MINGW_TEST_JOBS)'

check-local: windows-check

mingw-deps-clean:
	$(MAKE) -C vendor/mingw mingw-deps-clean
endif

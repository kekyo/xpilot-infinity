dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([XPilot Infinity], 4.7.3, xpilot-hacks@lists.sourceforge.net, xpilot-infinity)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/client/xpclient.h)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

windows_target=no
mingw_arch=
case "$host" in
	i686-w64-mingw32)
		windows_target=yes
		mingw_arch=x86
		;;
	x86_64-w64-mingw32)
		windows_target=yes
		mingw_arch=x86_64
		;;
	*-*-mingw32*)
		AC_MSG_ERROR([unsupported MinGW host: $host])
		;;
esac

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

dnl  Copyright & stuff.
XP_AUTHORS="Bjørn Stabell, Ken Ronny Schouten, Bert Gijsbers, Dick Balaska, Uoti Urpala, Juha Lindström, Kristian Söderblom and Erik Andersson"
AC_SUBST([XP_AUTHORS])

XP_COPYRIGHTDATE="1991-2005"
AC_SUBST([XP_COPYRIGHTDATE])

XP_COPYRIGHT="Copyright © $XP_COPYRIGHTDATE by $XP_AUTHORS"
AC_SUBST([XP_COPYRIGHT])

AC_SUBST([XP_RELEASEDATE],["Sep 16, 2005"])
AC_SUBST([XP_DATADIR],"$prefix/share/xpilot-infinity")

dnl  Checks for options.
dnl  We should have some general macro for these.
AC_ARG_ENABLE(dbe,
	AC_HELP_STRING([--enable-dbe],
			[support for X Doublebuffer Extension in client]),
[if test x$enable_dbe = xyes; then
	AC_DEFINE([DBE], 1, [support for X Doublebuffer Extension in client])
fi])

AC_ARG_ENABLE(mbx,
	AC_HELP_STRING([--enable-mbx],
			[support for X Multibuffer Extension in client]),
[if test x$enable_mbx = xyes; then
	AC_DEFINE([MBX], 1, [support for X Multibuffer Extension in client])
fi])

AC_ARG_ENABLE(plockserver,
	AC_HELP_STRING([--enable-plockserver],
			[lock the xpilot-infinity server into memory]),
[if test x$enable_plockserver = xyes; then
	AC_DEFINE([PLOCKSERVER], 1, [lock the xpilot-infinity server into memory])
fi])

AC_ARG_ENABLE(development,
	AC_HELP_STRING([--enable-development],
			[enable development code]),
[if test x$enable_development = xyes; then
	AC_DEFINE([DEVELOPMENT], 1, [enable development code])
fi])

AC_ARG_ENABLE(select_sched,
	AC_HELP_STRING([--enable-select-sched],
		[enable new server scheduling most useful on Linux 2.6]))

AC_ARG_ENABLE(replay,
	AC_HELP_STRING([--disable-replay],
			[disable building of replay program]),
[], [enable_replay=yes])
if test x$enable_replay = xyes; then
	need_x=yes
fi

AC_ARG_ENABLE(xp_mapedit,
	AC_HELP_STRING([--disable-xp-mapedit],
			[disable building of xp-mapedit program]),
[], [enable_xp_mapedit=yes])
if test x$enable_xp_mapedit = xyes; then
	need_x=yes
fi

AC_ARG_ENABLE(x11_client,
	AC_HELP_STRING([--disable-x11-client],
			[disable X11 client]),
[], [enable_x11_client=yes])
if test x$enable_x11_client = xyes; then
	need_x=yes
	need_libxpclient=yes
fi

AC_ARG_ENABLE(sdl_client,
	AC_HELP_STRING([--disable-sdl-client],
			[disable SDL/OpenGL client]),
[], [enable_sdl_client=yes])
if test x$enable_sdl_client = xyes; then
	need_libxpclient=yes
	AC_DEFINE([SDL_CLIENT], 1, [enable sdl client])
fi

AC_ARG_ENABLE(mingw_vendored_deps,
	AC_HELP_STRING([--disable-mingw-vendored-deps],
			[use externally prepared dependencies for MinGW]),
[], [
	if test x$windows_target = xyes; then
		enable_mingw_vendored_deps=yes
	else
		enable_mingw_vendored_deps=no
	fi
])
case "$enable_mingw_vendored_deps" in
	yes|no)
		;;
	*)
		AC_MSG_ERROR([--enable-mingw-vendored-deps accepts yes or no])
		;;
esac
if test x$windows_target != xyes \
	&& test x$enable_mingw_vendored_deps = xyes; then
	AC_MSG_ERROR([--enable-mingw-vendored-deps requires a MinGW host])
fi

AC_ARG_WITH(sdl3,
	AC_HELP_STRING([--with-sdl3=PROVIDER],
			[use system or vendored SDL3 dependencies]),
[], [
	if test x$enable_mingw_vendored_deps = xyes; then
		with_sdl3=vendored
	else
		with_sdl3=system
	fi
])
case "$with_sdl3" in
	system|vendored)
		;;
	*)
		AC_MSG_ERROR([--with-sdl3 must be system or vendored])
		;;
esac

AC_ARG_WITH(sdl3_prefix,
	AC_HELP_STRING([--with-sdl3-prefix=PATH],
			[private SDL3 prefix built by vendor/sdl3/build.sh]),
[], [with_sdl3_prefix=])

AC_ARG_WITH(mingw_toolchain_file,
	AC_HELP_STRING([--with-mingw-toolchain-file=PATH],
			[CMake toolchain file for MinGW vendored dependencies]),
[], [with_mingw_toolchain_file=])

AC_ARG_WITH(mingw_deps_build_type,
	AC_HELP_STRING([--with-mingw-deps-build-type=TYPE],
			[CMake build type for MinGW dependencies @<:@Release@:>@]),
[], [with_mingw_deps_build_type=Release])
test -n "$with_mingw_deps_build_type" \
	|| AC_MSG_ERROR([--with-mingw-deps-build-type requires a value])

mingw_vendored_deps=no
MINGW_ARCH=
MINGW_DEPS_BUILD_DIR=
MINGW_DEPS_PREFIX=
MINGW_DEPS_MAKEFILE=
MINGW_DEPS_BUILD_TYPE=
MINGW_TOOLCHAIN_FILE=
MINGW_PACKAGE_DIR=
MINGW_WINE_PREFIX=
if test x$windows_target = xyes; then
	MINGW_ARCH=$mingw_arch
	MINGW_PACKAGE_DIR="$ac_pwd/package"
	MINGW_WINE_PREFIX="$ac_pwd/wine-prefix"
fi
if test x$windows_target = xyes \
	&& test x$enable_mingw_vendored_deps = xyes; then
	mingw_vendored_deps=yes
	if test x$with_sdl3 != xvendored; then
		AC_MSG_ERROR([MinGW vendored dependencies require --with-sdl3=vendored])
	fi
	if test -z "$with_sdl3_prefix"; then
		with_sdl3_prefix="$ac_pwd/mingw-vendor-prefix"
	fi
	case "$with_sdl3_prefix" in
		/*)
			;;
		*)
			AC_MSG_ERROR([--with-sdl3-prefix must be an absolute path])
			;;
	esac

	xpilot_abs_srcdir=`cd "$srcdir" && pwd`
	if test -z "$with_mingw_toolchain_file"; then
		case "$mingw_arch" in
			x86)
				with_mingw_toolchain_file="$xpilot_abs_srcdir/vendor/sdl3/SDL/build-scripts/cmake-toolchain-mingw64-i686.cmake"
				;;
			x86_64)
				with_mingw_toolchain_file="$xpilot_abs_srcdir/vendor/sdl3/SDL/build-scripts/cmake-toolchain-mingw64-x86_64.cmake"
				;;
		esac
	fi
	case "$with_mingw_toolchain_file" in
		/*)
			;;
		*)
			AC_MSG_ERROR([--with-mingw-toolchain-file must be an absolute path])
			;;
	esac
	test -f "$with_mingw_toolchain_file" \
		|| AC_MSG_ERROR([MinGW toolchain file not found: $with_mingw_toolchain_file])

	MINGW_DEPS_BUILD_DIR="$ac_pwd/mingw-vendor-build"
	MINGW_DEPS_PREFIX=$with_sdl3_prefix
	MINGW_DEPS_MAKEFILE="$ac_pwd/mingw-deps.mk"
	MINGW_DEPS_BUILD_TYPE=$with_mingw_deps_build_type
	MINGW_TOOLCHAIN_FILE=$with_mingw_toolchain_file
fi

AC_SUBST([MINGW_ARCH])
AC_SUBST([MINGW_DEPS_BUILD_DIR])
AC_SUBST([MINGW_DEPS_PREFIX])
AC_SUBST([MINGW_DEPS_MAKEFILE])
AC_SUBST([MINGW_DEPS_BUILD_TYPE])
AC_SUBST([MINGW_TOOLCHAIN_FILE])
AC_SUBST([MINGW_PACKAGE_DIR])
AC_SUBST([MINGW_WINE_PREFIX])

AC_ARG_ENABLE(sound,
	AC_HELP_STRING([--disable-sound],
			[disable OpenAL sound]),
[], [enable_sound=yes])
case "$enable_sound" in
	yes|no)
		;;
	*)
		AC_MSG_ERROR([--enable-sound accepts yes or no])
		;;
esac

dnl  Checks for programs.
AC_PROG_CC
dnl AC_PROG_GCC_TRADITIONAL
dnl AC_PROG_CPP

dnl Needed if you want to compile with g++ e.g.
dnl AC_PROG_CXX
dnl AC_PROG_CXXCPP

dnl AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE([server-tray],
    AS_HELP_STRING([--enable-server-tray=auto|yes|no],
        [build the independent desktop service controller (default: auto)]),
    [], [enable_server_tray=auto])
case "$enable_server_tray" in
    auto|yes|no) ;;
    *) AC_MSG_ERROR([--enable-server-tray accepts auto, yes or no]) ;;
esac
server_tray=no
if test x$enable_server_tray != xno; then
    if test x$windows_target = xyes; then
        server_tray=yes
    else
        case "$host_os" in
            linux*)
                PKG_CHECK_MODULES([TRAY], [gtk+-3.0 >= 3.22 gio-2.0 >= 2.60 gdk-x11-3.0 x11 xfixes],
                    [PKG_CHECK_MODULES([TRAY_POLKIT], [polkit-gobject-1],
                        [server_tray=yes], [server_tray=no])],
                    [server_tray=no])
                ;;
        esac
    fi
fi
if test x$enable_server_tray = xyes && test x$server_tray != xyes; then
    AC_MSG_ERROR([server tray requires Linux GTK >= 3.22, GIO >= 2.60, GDK X11, Xlib and polkit development packages, or a supported MinGW host])
fi
AM_CONDITIONAL([COND_SERVER_TRAY], [test x$server_tray = xyes])
AC_MSG_NOTICE([Server tray support: $server_tray (requested: $enable_server_tray)])

if test x$mingw_vendored_deps = xyes; then
	AC_PATH_PROG([CMAKE], [cmake])
	test -n "$CMAKE" \
		|| AC_MSG_ERROR([cmake is required for MinGW vendored dependencies])
	test -n "$PKG_CONFIG" \
		|| AC_MSG_ERROR([pkg-config is required for MinGW vendored dependencies])
fi

dnl  Checks for libraries.
dnl kps - these should be cleaned up and not use the autoconf internal
dnl shell variables.
if test x$mingw_vendored_deps = xyes; then
	AC_DEFINE([HAVE_LIBM], 1,
		[Define to 1 if you have the `m' library (-lm).])
	AC_DEFINE([HAVE_LIBZ], 1,
		[Define to 1 if you have the `z' library (-lz).])
	AC_DEFINE([HAVE_LIBEXPAT], 1,
		[Define to 1 if you have the `expat' library (-lexpat).])
else
	AC_CHECK_LIB(m, cos)
	if test x$ac_cv_lib_m_cos != xyes; then
		AC_MSG_ERROR([*** Math library not found!])
	fi
	AC_CHECK_LIB(z, gzopen)
	if test x$ac_cv_lib_z_gzopen != xyes; then
		AC_MSG_ERROR([*** Required library zlib not found!])
	fi
	AC_CHECK_LIB(expat, XML_ParserCreate)
	if test x$ac_cv_lib_expat_XML_ParserCreate != xyes; then
		AC_MSG_ERROR([*** Required library Expat not found!])
	fi
fi

W32_LIBS=
if test x$windows_target = xyes; then
	W32_LIBS="-lmingw32 -lws2_32 -ladvapi32"
	AC_CHECK_TOOL([WINDRES], [windres])
	AC_DEFINE([_WINDOWS], 1, [compiling for windows target])

	enable_select_sched=yes
	enable_replay=no
	enable_xp_mapedit=no
	enable_x11_client=no
	need_x=no
fi
AC_SUBST([W32_LIBS])

AC_PATH_X
if test x$need_x = xyes; then
	if test x$no_x == xyes; then
		AC_MSG_ERROR([*** Couldn't find X headers or libraries!])
	fi
	
	AC_PATH_XTRA

	AC_MSG_CHECKING(for xf86misc extensions)
	AC_COMPILE_IFELSE([
	 #include <X11/X.h>
	 #include <X11/Xlib.h>
	 #include <X11/extensions/xf86misc.h>
	 int main(int argc, char** argv) { return 0; }
	],[
	have_xf86misc=yes
	],[
	have_xf86misc=no])
	AC_MSG_RESULT($have_xf86misc)

	dnl kps - Hack to link with libXext if DBE or MBX is configured
	if test x$enable_dbe = xyes || test x$enable_mbx = xyes || test x$have_xf86misc = xyes; then
		X_EXTENSIONS_LIB="-lXext"
	else
		X_EXTENSIONS_LIB=
	fi
	
	if test x$have_xf86misc = xyes; then
		X_EXTENSIONS_LIB="-lXxf86misc $X_EXTENSIONS_LIB"
		AC_DEFINE([HAVE_XF86MISC], 1, [Define to 1 if you have the Xf86Misc library])
	fi
	
	AC_SUBST(X_EXTENSIONS_LIB)
fi

if test x$enable_x11_client = xyes; then
	AC_DEFINE([X11_CLIENT], 1, [enable x11 client])
fi

if test x$enable_select_sched = xyes; then
	AC_DEFINE([SELECT_SCHED], 1,
		[enable new server scheduling most useful on Linux 2.6])
fi

if test x$enable_replay = xyes; then
	AC_DEFINE([REPLAY], 1, [enable replay program])
fi

if test x$enable_xp_mapedit = xyes; then
	AC_DEFINE([XP_MAPEDIT], 1, [enable xp-mapedit program])
fi

dnl SDL3 and SDL_ttf provide the shared native text implementation used by
dnl both graphical clients. SDL_image and OpenGL remain SDL-client-only.
if test x$enable_sdl_client = xyes || test x$enable_x11_client = xyes; then
	if test x$mingw_vendored_deps = xyes; then
		SDL3_CFLAGS=
		SDL3_LIBS=
		SDL3_IMAGE_CFLAGS=
		SDL3_IMAGE_LIBS=
		SDL3_TTF_CFLAGS=
		SDL3_TTF_LIBS=
		GL_CFLAGS=-DWIN32
		GL_LIBS=-lopengl32
	else
		if test x$with_sdl3 = xvendored; then
			if test -z "$with_sdl3_prefix"; then
				AC_MSG_ERROR([--with-sdl3=vendored requires --with-sdl3-prefix=PATH])
			fi
			case "$with_sdl3_prefix" in
				/*)
					;;
				*)
					AC_MSG_ERROR([--with-sdl3-prefix must be an absolute path])
					;;
			esac
			if test ! -d "$with_sdl3_prefix/lib/pkgconfig"; then
				AC_MSG_ERROR([vendored SDL3 pkg-config directory not found: $with_sdl3_prefix/lib/pkgconfig])
			fi
			for sdl3_pc_file in sdl3.pc sdl3-ttf.pc; do
				if test ! -f "$with_sdl3_prefix/lib/pkgconfig/$sdl3_pc_file"; then
					AC_MSG_ERROR([vendored SDL3 metadata not found: $with_sdl3_prefix/lib/pkgconfig/$sdl3_pc_file])
				fi
			done
			if test x$enable_sdl_client = xyes \
				&& test ! -f "$with_sdl3_prefix/lib/pkgconfig/sdl3-image.pc"; then
				AC_MSG_ERROR([vendored SDL3 metadata not found: $with_sdl3_prefix/lib/pkgconfig/sdl3-image.pc])
			fi

			if test "${PKG_CONFIG_PATH+set}" = set; then
				saved_sdl3_pkg_config_path=$PKG_CONFIG_PATH
				sdl3_pkg_config_path_was_set=yes
				PKG_CONFIG_PATH="$with_sdl3_prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
			else
				sdl3_pkg_config_path_was_set=no
				PKG_CONFIG_PATH="$with_sdl3_prefix/lib/pkgconfig"
			fi
			export PKG_CONFIG_PATH
		fi

		PKG_CHECK_MODULES([SDL3], [sdl3 >= 3.2.0])
		PKG_CHECK_MODULES([SDL3_TTF], [sdl3-ttf >= 3.2.0])
		if test x$enable_sdl_client = xyes; then
			PKG_CHECK_MODULES([SDL3_IMAGE], [sdl3-image >= 3.2.0])
			PKG_CHECK_MODULES([GL], [gl])
		else
			SDL3_IMAGE_CFLAGS=
			SDL3_IMAGE_LIBS=
			GL_CFLAGS=
			GL_LIBS=
		fi

		if test x$with_sdl3 = xvendored; then
			AC_MSG_CHECKING([for static vendored SDL3 linker flags])
			SDL3_LIBS=`$PKG_CONFIG --static --libs sdl3`
			if test $? -ne 0; then
				AC_MSG_ERROR([could not resolve static SDL3 linker flags])
			fi
			SDL3_TTF_LIBS=`$PKG_CONFIG --static --libs sdl3-ttf`
			if test $? -ne 0; then
				AC_MSG_ERROR([could not resolve static SDL3_ttf linker flags])
			fi
			if test x$enable_sdl_client = xyes; then
				SDL3_IMAGE_LIBS=`$PKG_CONFIG --static --libs sdl3-image`
				if test $? -ne 0; then
					AC_MSG_ERROR([could not resolve static SDL3_image linker flags])
				fi
			fi
			AC_MSG_RESULT([yes])

			if test x$sdl3_pkg_config_path_was_set = xyes; then
				PKG_CONFIG_PATH=$saved_sdl3_pkg_config_path
				export PKG_CONFIG_PATH
			else
				unset PKG_CONFIG_PATH
			fi
		fi
	fi
	AC_MSG_NOTICE([SDL3 dependency provider: $with_sdl3])
	if test x$enable_sdl_client = xyes; then
	AC_DEFINE([HAVE_SDL_IMAGE], 1,
		  [Define to 1 if you have the SDL_image library.])
	fi
fi

NATIVE_TEXT_CFLAGS=
NATIVE_TEXT_LIBS=
if test x$enable_sdl_client = xyes || test x$enable_x11_client = xyes; then
	if test x$windows_target = xyes; then
		NATIVE_TEXT_LIBS="-lgdi32"
	else
		PKG_CHECK_MODULES([FONTCONFIG], [fontconfig >= 2.8.0])
		NATIVE_TEXT_CFLAGS=$FONTCONFIG_CFLAGS
		NATIVE_TEXT_LIBS=$FONTCONFIG_LIBS
	fi
fi

XRENDER_CFLAGS=
XRENDER_LIBS=
if test x$enable_x11_client = xyes; then
	PKG_CHECK_MODULES([XRENDER], [xrender >= 0.9.0])
fi

AC_SUBST([SDL3_CFLAGS])
AC_SUBST([SDL3_LIBS])
AC_SUBST([SDL3_IMAGE_CFLAGS])
AC_SUBST([SDL3_IMAGE_LIBS])
AC_SUBST([SDL3_TTF_CFLAGS])
AC_SUBST([SDL3_TTF_LIBS])
AC_SUBST([NATIVE_TEXT_CFLAGS])
AC_SUBST([NATIVE_TEXT_LIBS])
AC_SUBST([XRENDER_CFLAGS])
AC_SUBST([XRENDER_LIBS])
AC_SUBST([GL_CFLAGS])
AC_SUBST([GL_LIBS])

AM_CONDITIONAL([COND_CLIENT], [test x$need_libxpclient = xyes])
AM_CONDITIONAL([COND_WINDOWS], [test x$windows_target = xyes])
AM_CONDITIONAL([COND_MINGW_VENDORED], [test x$mingw_vendored_deps = xyes])
AM_CONDITIONAL([COND_REPLAY], [test x$enable_replay = xyes])
AM_CONDITIONAL([COND_XP_MAPEDIT], [test x$enable_xp_mapedit = xyes])
AM_CONDITIONAL([COND_X11_CLIENT], [test x$enable_x11_client = xyes])
AM_CONDITIONAL([COND_SDL_CLIENT], [test x$enable_sdl_client = xyes])

dnl OpenAL client sound support. MinGW dependencies are built by make after
dnl configure, so their compiler and linker flags come from mingw-deps.mk.
have_sound=no
SOUND_CFLAGS=
SOUND_LIBS=
if test x$enable_sound = xyes && test x$need_libxpclient = xyes; then
	if test x$mingw_vendored_deps = xyes; then
		have_sound=yes
	else
		PKG_CHECK_MODULES([SOUND], [freealut openal], [], [
			AC_MSG_ERROR([OpenAL and freealut are required for client sound; install their development packages or use --disable-sound])
		])
		have_sound=yes
	fi
	AC_DEFINE([SOUND], 1, [Define to 1 if you want OpenAL sound.])
fi
AC_SUBST([SOUND_CFLAGS])
AC_SUBST([SOUND_LIBS])

AM_CONDITIONAL([COND_SOUND], [test x$have_sound = xyes])

dnl  Checks for header files.
dnl AC_HEADER_STDC
AC_HEADER_TIME
dnl AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	arpa/inet.h \
	arpa/nameser.h \
	assert.h \
	bstring.h \
	ctype.h \
	errno.h \
	fcntl.h \
	float.h \
	limits.h \
	math.h \
	net/if.h \
	netdb.h \
	netinet/in.h \
	netinet/tcp.h \
	pwd.h \
	resolv.h \
	setjmp.h \
	signal.h \
	stdarg.h \
	stddef.h \
	sys/file.h \
	sys/filio.h \
	sys/fcntl.h \
	sys/inttypes.h \
	sys/ioctl.h \
	sys/lock.h \
	sys/mman.h \
	sys/param.h \
	sys/socket.h \
	sys/sockio.h \
	sys/time.h \
	values.h \
	X11/X.h \
	X11/Xlib.h \
	X11/Xos.h \
	X11/Xutil.h \
	X11/keysym.h \
	X11/Xatom.h \
	X11/Xmd.h \
	])

dnl 	X11/extensions/Xdbe.h
dnl	X11/extensions/multibuf.h

dnl  Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_TYPE_MODE_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Check for socklen_t (in Unix98)
if test x$windows_target = xyes; then
	AC_MSG_CHECKING(for socklen_t)
	AC_MSG_RESULT([provided by Winsock2])
else
	AC_MSG_CHECKING(for socklen_t)
	AC_TRY_COMPILE([
		#include <sys/types.h>
		#include <sys/socket.h>
		socklen_t x;
	], [],
	[
		AC_MSG_RESULT(yes)
	], [
		AC_TRY_COMPILE([
			#include <sys/types.h>
			#include <sys/socket.h>
			int accept(int, struct sockaddr *, size_t *);
		], [], [
			AC_MSG_RESULT(size_t)
			AC_DEFINE(socklen_t, size_t, [socklen_t size])
		], [
			AC_MSG_RESULT(int)
			AC_DEFINE(socklen_t, int, [socklen_t size])
		])
	])
fi

dnl  Checks for library functions.

dnl We probably don't need GNU compatible malloc and realloc functions,
dnl so these are disabled for now.
dnl AC_FUNC_MALLOC
dnl AC_FUNC_REALLOC

AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_SETVBUF_REVERSED
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ \
	alarm \
	floor \
	gethostbyaddr \
	gethostbyname \
	gethostname \
	gettimeofday \
	inet_ntoa \
	isascii \
	memchr \
	memmove \
	memset \
	mkdir \
	pow \
	rint \
	select \
	socket \
	sqrt \
	strcasecmp \
	strchr \
	strdup \
	strerror \
	strlcat \
	strlcpy \
	strncasecmp \
	strpbrk \
	strrchr \
	strspn \
	strstr \
	strtol \
	strtoul \
	])

dnl MinGW vendored libraries are built by make, after configure has finished.
dnl Add their final locations only after configure's compile and link probes.
if test x$mingw_vendored_deps = xyes; then
	CPPFLAGS="-I$MINGW_DEPS_PREFIX/include $CPPFLAGS"
	LDFLAGS="-L$MINGW_DEPS_PREFIX/lib $LDFLAGS"
	LIBS="-lexpat -lz -lm $LIBS"
fi

AC_CONFIG_FILES([ \
	Makefile \
	contrib/Makefile \
	contrib/xpicc/Makefile \
	doc/Makefile \
	doc/man/Makefile \
	lib/Makefile \
	lib/fonts/Makefile \
	lib/maps/Makefile \
	lib/textures/Makefile \
	lib/sound/Makefile \
	src/Makefile \
	src/common/Makefile \
	src/common/version.h.template:src/common/version.h.in \
	src/client/Makefile \
	src/client/items/Makefile \
	src/client/sdl/Makefile \
	src/client/x11/Makefile \
	src/server/Makefile \
	src/tray/Makefile \
	src/tray/tests/Makefile \
	src/replay/Makefile \
	src/replay/tools/Makefile \
	src/mapedit/Makefile \
	tests/Makefile \
	vendor/mingw/Makefile \
	])
AC_OUTPUT

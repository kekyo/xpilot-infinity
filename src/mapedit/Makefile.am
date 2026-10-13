bin_PROGRAMS = xpilot-infinity-xp-mapedit
AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" \
	-I$(top_builddir)/src/common -I$(top_srcdir)/src/common
xpilot_infinity_xp_mapedit_SOURCES = default_colors.h defaults.h events.c expose.c file.c file.h forms.c grow.c help.c main.c main.h proto.h round.c T_Form.c T_Handler.c tools.c T_Popup.c T_Toolkit.c T_Toolkit.h xpmapedit.h
xpilot_infinity_xp_mapedit_LDADD = $(top_builddir)/src/common/libxpcommon.a @X_LIBS@ @X_PRE_LIBS@ @X_EXTRA_LIBS@ -lX11
EXTRA_DIST = COPYRIGHT README README.UPDATE TODO

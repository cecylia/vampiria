AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(includedir)					\
	$(PTHREAD_LIBS)					\
	$(M_LIBS)					\
	$(ALSA_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(GSTREAMER_CFLAGS)				\
	$(XML_CFLAGS)					\
	-DG_LOG_DOMAIN=\"gnomeradio\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)/"\"			\
	-DPREFIX=\""$(prefix)/"\"			\
	-DLIBDIR=\""$(libdir)/"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)/"\"

bin_PROGRAMS = gnomeradio

GNOMERADIO_RESOURCES = \
	gnomeradio-dtd-resources.c \
	gnomeradio-dtd-resources.h

gnomeradio_SOURCES =		\
	alsa_stream.h		\
	gui.h			\
	lirc.h			\
	media_devices.h		\
	media_types.h		\
	messages.h		\
	missing_plugins.h	\
	mixer.h			\
	prefs.h			\
	radio.h			\
	record.h		\
	rec_tech.h		\
	trayicon.h		\
	utils.h			\
	v4l1.h			\
	v4l2.h			\
	alsa_stream.c		\
	gui.c			\
	lirc.c			\
	media_devices.c		\
	media_types.c		\
	messages.c		\
	missing_plugins.c	\
	mixer.c			\
	mixer_alsa.c		\
	mixer_oss.c		\
	prefs.c			\
	radio.c			\
	record.c		\
	rec_tech.c		\
	trayicon.c		\
	utils.c			\
	v4l1.c			\
	v4l2.c			\
	$(GNOMERADIO_RESOURCES)

RESOURCE_FILES = \
	resources/gnomeradio.dtd

EXTRA_DIST = \
	gnomeradio-dtd.gresource.xml \
	$(RESOURCE_FILES)

gnomeradio-dtd-resources.c: gnomeradio-dtd.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-source --c-name gnomeradio $(srcdir)/gnomeradio-dtd.gresource.xml

gnomeradio-dtd-resources.h: gnomeradio-dtd.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-header --c-name gnomeradio $(srcdir)/gnomeradio-dtd.gresource.xml

BUILT_SOURCES = \
	$(GNOMERADIO_RESOURCES)

gnomeradio_LDADD = $(ALSA_LIBS) $(GNOME_LIBS) $(GSTREAMER_LIBS) $(XML_LIBS) $(LIRC)

CLEANFILES = $(BUILT_SOURCES)

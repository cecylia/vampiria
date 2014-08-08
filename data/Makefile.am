SUBDIRS = icons pixmaps

applicationsdir = $(datadir)/applications/
applications_in_files = gnomeradio.desktop.in
applications_DATA = $(applications_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

all-local: $(applications_DATA)

profilesdir = $(datadir)/gnomeradio
profiles_DATA = gnomeradio.gep

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_SCHEMAS = org.gnome.gnomeradio.gschema.xml

@GSETTINGS_RULES@

man_MANS = gnomeradio.1

EXTRA_DIST =				\
	$(applications_DATA)		\
	$(applications_in_files)	\
	$(profiles_DATA)		\
	$(gsettings_SCHEMAS)		\
	$(man_MANS)			\
	example.lircrc

CLEANFILES = $(applications_DATA)



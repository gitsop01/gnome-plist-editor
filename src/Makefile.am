bin_PROGRAMS = gnome-plist-editor

gnome_plist_editor_SOURCES = \
	gnome-plist-editor.c

gnome_plist_editor_CFLAGS = \
	-Wl,--export-dynamic \
	$(GLIB_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(LIBPLIST_CFLAGS)

gnome_plist_editor_LDADD = \
	-lpthread \
	$(GLIB_LIBS) \
	$(GTK3_LIBS) \
	$(LIBPLIST_LIBS)

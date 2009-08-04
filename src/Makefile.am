bin_PROGRAMS = gnome-plist-editor

gnome_plist_editor_SOURCES = \
	gnome-plist-editor.c \
	plist-utils.c

gnome_plist_editor_CFLAGS = \
	-Wl,--export-dynamic \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBPLIST_CFLAGS)

gnome_plist_editor_LDADD = \
	-lpthread \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBPLIST_LIBS)

/*
 * gnome-plist-editor.c
 *
 * Copyright (C) 2009 Martin Szulecki <opensuse@sukimashita.com>
 * 
 * The code contained in this file is free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either version
 * 2.1 of the License, or (at your option) any later version.
 *  
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this code; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include <config.h>

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

#include <plist/plist.h>
#include "plist-utils.h"

static struct AppState {
	GtkWindow *main_window;
	GtkTreeStore *document_tree_store;
	GtkTreeView *document_tree_view;

	plist_t root_node;
	gboolean is_binary;
} app;

void main_window_destroy_cb(GtkWidget* widget, gpointer user_data) {
	if (app.document_tree_store)
		gtk_tree_store_clear(app.document_tree_store);

	if (app.root_node)
		plist_free(app.root_node);

	gtk_main_quit();
}

void update_document_tree_view(plist_t node, GtkTreeIter *parent) {
	GtkTreeStore *store = app.document_tree_store;
	GtkTreeIter iter;
	plist_t subnode = NULL;
	plist_type type;

	gtk_tree_store_append(store, &iter, parent);
	gtk_tree_store_set(store, &iter,
		0, (gpointer)node,
		-1);

	/* If structured node, recurse through childrens (skip keys in case of dicts) */
	type = plist_get_node_type(node);
	if ( type == PLIST_ARRAY ) {
		uint32_t i = 0;
		uint32_t size = plist_array_get_size(node);
		for (i = 0; i < size; i++) {
			subnode = plist_array_get_item(node, i);
			update_document_tree_view(subnode, &iter);
		}
	}
	else if ( type == PLIST_DICT ) {
		plist_dict_iter dict_iter = NULL;
		plist_dict_new_iter(node, &dict_iter);

		plist_dict_next_item(node, dict_iter, NULL, &subnode);
		while (subnode) {
			update_document_tree_view(subnode, &iter);
			plist_dict_next_item(node, dict_iter, NULL, &subnode);
		};

		free(dict_iter);
	}
}

static void open_plist_file(const char *filename) {
	char *buffer = NULL;
	gsize size = 0;
	gsize bytes_read = 0;
	GFile *file = NULL;
	GFileInfo *fileinfo = NULL;
	GFileInputStream *filestream = NULL;
	GError *err = NULL;

	file = g_file_new_for_path(filename);
	filestream = g_file_read(file, NULL, &err);
	if (!filestream) {
		fprintf(stderr, "ERROR: opening %s: %s\n", file, err->message);
		g_object_unref(file);
		goto leave;
	}

	fileinfo = g_file_query_info(file,
		G_FILE_ATTRIBUTE_STANDARD_SIZE,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&err);

	size = g_file_info_get_size(fileinfo);

	/* read in the plist */
	buffer = (char *)g_malloc(sizeof(char) * (size + 1));
	g_input_stream_read_all(G_INPUT_STREAM(filestream),
		buffer,
		size,
		&bytes_read,
		NULL,
		&err);

	if (memcmp(buffer, "bplist00", 8) == 0) {
		plist_from_bin(buffer, size, &app.root_node);
		app.is_binary = TRUE;
	} else {
		plist_from_xml(buffer, size, &app.root_node);
		app.is_binary = FALSE;
	}

	gtk_tree_store_clear(app.document_tree_store);
	update_document_tree_view(app.root_node, NULL);
	gtk_tree_view_expand_all(app.document_tree_view);

	g_free(buffer);

leave:
	if (fileinfo)
		g_object_unref(fileinfo);
	if (filestream)
		g_object_unref(filestream);
	if (file)
		g_object_unref(file);
}

void open_plist_cb(GtkWidget* item, gpointer user_data) {
	GtkWidget *dialog;
	GtkFileFilter *filter;

	char *filename = NULL;

	dialog = gtk_file_chooser_dialog_new ("Open Property List File",
		app.main_window,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	/* add default filter */
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All Files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	/* add plist filter */
	filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "*.plist");
	gtk_file_filter_set_name(filter, "Property List Files");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		open_plist_file(filename);
	}

	if (filename)
		g_free (filename);

	gtk_widget_destroy (dialog);
}

void about_menu_item_activate_cb(GtkMenuItem* item, gpointer user_data) {
	gtk_show_about_dialog(app.main_window,
		"program-name", "GNOME Property List Editor",
		"logo-icon-name", "text-editor",
		"website", "http://cgit.sukimashita.com/gnome-plist-editor.git/",
		"website-label", "GIT Source Code Repository",
		"version", PACKAGE_VERSION,
		NULL);
}

typedef enum {
	COL_KEY,
	COL_TYPE,
	COL_VALUE,
	N_COLUMNS
} col_type_t;

void plist_cell_data_function (GtkTreeViewColumn *col,
	GtkCellRenderer *renderer,
	GtkTreeModel *model,
	GtkTreeIter *iter,
	gpointer user_data)
{
	col_type_t col_type;
	plist_t node;
	plist_t parent_node;
	plist_type value_type;
	plist_type parent_type;
	char *text = NULL;

	char *s = NULL;
	double d;
	uint8_t b;
	uint64_t u = 0;

	col_type = (col_type_t)GPOINTER_TO_INT(user_data);

	gtk_tree_model_get(model, iter, 0, &node, -1);

	parent_node = plist_get_parent(node);
	parent_type = plist_get_node_type(parent_node);

	value_type = plist_get_node_type(node);

	switch(col_type) {
	case COL_KEY:
		if (parent_node == NULL) {
			text = "Root";
		}
		else if (parent_type == PLIST_DICT) {
			plist_dict_get_item_key(node, &text);
		}
		else if (parent_type == PLIST_ARRAY) {
			int index = plist_array_get_item_index(node);
			text = g_strdup_printf("Item %i", index);
		}
		break;
	case COL_TYPE:
		switch(value_type) {
		case PLIST_BOOLEAN:
			text = "Boolean";
			break;
		case PLIST_UINT:
			text = "Number";
			break;
		case PLIST_REAL:
			text = "Float";
			break;
		case PLIST_STRING:
			text = "String";
			break;
		case PLIST_DATA:
			text = "Data";
			break;
		case PLIST_DATE:
			text = "Date";
			break;
		case PLIST_ARRAY:
			text = "Array";
			break;
		case PLIST_DICT:
			text = "Dictionary";
			break;
		}
		break;
	case COL_VALUE:
		g_object_set(renderer, "sensitive", TRUE, NULL);
		switch(value_type) {
		case PLIST_BOOLEAN:
			plist_get_bool_val(node, &b);
			text = (b ? "true" : "false");
			break;
		case PLIST_UINT:
			plist_get_uint_val(node, &u);
			text = g_strdup_printf("%llu", (long long)u);
			break;
		case PLIST_REAL:
			plist_get_real_val(node, &d);
			text = g_strdup_printf("%f", d);
			break;
		case PLIST_STRING:
			plist_get_string_val(node, &text);
			break;
		case PLIST_DATA:
			text = "FIXME: Parse Data";
			break;
		case PLIST_DATE:
			text = "FIXME: Parse Dates";
			break;
		case PLIST_ARRAY:
		case PLIST_DICT:
			text = g_strdup_printf("(%d items)", plist_node_get_item_count(node));
			g_object_set(renderer, "sensitive", FALSE, NULL);
			break;
		default:
			break;
		}
		break;
	}

	g_object_set(renderer, "text", text, NULL);
}

void setup_tree_view(GtkBuilder *gtk_builder) {
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;

	app.document_tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gtk_builder, "document_tree_view"));
	app.document_tree_store = GTK_TREE_STORE(gtk_builder_get_object(gtk_builder, "document_tree_store"));

	/* key */
	column = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(gtk_builder, "treeviewcolumn1"));
	cell = GTK_CELL_RENDERER(gtk_builder_get_object(gtk_builder, "cellrenderertext1"));
	gtk_tree_view_column_set_cell_data_func(column, cell, plist_cell_data_function, GINT_TO_POINTER(COL_KEY), NULL);

	/* type */
	column = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(gtk_builder, "treeviewcolumn2"));
	cell = GTK_CELL_RENDERER(gtk_builder_get_object(gtk_builder, "cellrenderercombo1"));
	gtk_tree_view_column_set_cell_data_func(column, cell, plist_cell_data_function, GINT_TO_POINTER(COL_TYPE), NULL);

	/* value */
	column = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(gtk_builder, "treeviewcolumn3"));
	cell = GTK_CELL_RENDERER(gtk_builder_get_object(gtk_builder, "cellrenderertext2"));
	gtk_tree_view_column_set_cell_data_func(column, cell, plist_cell_data_function, GINT_TO_POINTER(COL_VALUE), NULL);
}

int main(int argc, char **argv) {
	GtkBuilder *gtk_builder;

	gtk_set_locale();

	/* Initialize the widget set */
	gtk_init (&argc, &argv);

	/* Create the main window */
	gtk_builder = gtk_builder_new();
	gtk_builder_add_from_file(gtk_builder, "gnome-plist-editor.ui", NULL);
	gtk_builder_connect_signals(gtk_builder, NULL);

	app.main_window = GTK_WINDOW(gtk_builder_get_object(gtk_builder, "main_window"));
	setup_tree_view(gtk_builder);

	g_object_unref(G_OBJECT(gtk_builder));

	/* Show the application window */
	gtk_widget_show_all(GTK_WIDGET(app.main_window));

	/* Enter the main event loop, and wait for user interaction */
	gtk_main ();

	return 0;
}


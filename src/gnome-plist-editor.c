/*
 * gnome-plist-editor.c
 *
 * Copyright (C) 2009 Martin Szulecki <opensuse@sukimashita.com>
 * Copyright (C) 2016-2017 Timothy Ward <gtwa001@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335
 * USA
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtk-3.0/gtk/gtk.h>
#include <gio/gio.h>

#include <plist/plist.h>

static struct AppState {
	GtkWindow *main_window;
	GtkTreeStore *document_tree_store;
	GtkTreeView *document_tree_view;

	plist_t root_node;
	gboolean is_binary;
} app;

typedef enum {
	COL_KEY,
	COL_TYPE,
	COL_VALUE,
	N_COLUMNS
} col_type_t;

char *buffer = NULL;
gsize size = 0;
gsize bytes_read = 0;

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
/*	char *buffer = NULL; */

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

	plist_free(app.root_node);
	app.root_node = NULL;

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

/*	g_free(buffer); */

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

static void close_plist_file(const char *filename) {
/*	char *buffer = NULL; */
/*	gsize size = 0; */
/*	gsize bytes_read = 0; */
	GFile *file = NULL;
	GFileInfo *fileinfo = NULL;
	GFileOutputStream *filestream = NULL;
	GError *err = NULL;

	file = g_file_new_for_path(filename);
	filestream = g_file_create(file,G_FILE_CREATE_NONE, NULL, &err);
	if (!filestream) {
		fprintf(stderr, "ERROR: create %s: %s\n", file, err->message);
		g_object_unref(file);
		goto leave;
	}

	fileinfo = g_file_query_info(file,
		G_FILE_ATTRIBUTE_STANDARD_SIZE,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&err);

	size = g_file_info_get_size(fileinfo);

	/* write the plist to file*/
/*	buffer = (char *)g_malloc(sizeof(char) * (size + 1)); */
	gboolean  test = g_output_stream_write_all(G_OUTPUT_STREAM(filestream),
		buffer,
		size,
		&bytes_read,
		NULL,
		&err);
        if (err != NULL){
          fprintf(stderr," output stream error %c", err);
        }
        if ( test == FALSE){
          fprintf(stderr," output steam failure %c", err);
        }
	plist_free(app.root_node);
	app.root_node = NULL;

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

void close_plist_cb(GtkWidget* item, gpointer user_data) {
	GtkWidget *dialog;
	GtkFileFilter *filter;

	char *filename = NULL;

	dialog = gtk_file_chooser_dialog_new ("Close Property List File",
		app.main_window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
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
		close_plist_file(filename);
	}

	if (filename)
		g_free (filename);

	gtk_widget_destroy (dialog);
}


void new_plist_cb(GtkWidget* item, gpointer user_data) {

	gtk_tree_store_clear(app.document_tree_store);

	plist_free(app.root_node);
	app.root_node = plist_new_dict();
	update_document_tree_view(app.root_node, NULL);
	gtk_tree_view_expand_all(app.document_tree_view);
}

void add_child_cb(GtkWidget* item, gpointer user_data) {

	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeView      *view = GTK_TREE_VIEW(app.document_tree_view);
	GtkTreeIter       iter;
	GtkTreePath      *path = NULL;
	plist_t child = NULL;

	selection = gtk_tree_view_get_selection(view);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		plist_t node = NULL;
		gtk_tree_model_get (model, &iter, 0, &node, -1);

		//check that node is a structured node
		plist_type type = plist_get_node_type(node);

		if ( type == PLIST_ARRAY || type == PLIST_DICT ) {
			path = gtk_tree_model_get_path(model, &iter);

			//default child is of type bool
			child = plist_new_bool(0);

			if (type == PLIST_ARRAY) {
				plist_array_append_item(node, child);
			}
			else if (type == PLIST_DICT) {
				uint32_t n = plist_dict_get_size(node);
				gchar* key = g_strdup_printf("Item %i", n);
				plist_dict_set_item(node, key, child);
				g_free(key);
			}

			//update tree model
			GtkTreeIter iter_child;

			gtk_tree_store_append(app.document_tree_store, &iter_child, &iter);
			gtk_tree_store_set(app.document_tree_store, &iter_child,
				0, (gpointer)child,
				-1);
			gtk_tree_view_expand_row(view, path, FALSE);
			gtk_tree_path_free(path);
		}
	}
}

void remove_child_cb(GtkWidget* item, gpointer user_data) {

	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeView      *view = GTK_TREE_VIEW(app.document_tree_view);
	GtkTreeIter       iter;
	plist_t parent = NULL;

	selection = gtk_tree_view_get_selection(view);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		plist_t node = NULL;
		gtk_tree_model_get (model, &iter, 0, &node, -1);
		parent = plist_get_parent(node);

		if (parent) {
			plist_type type = plist_get_node_type(parent);

			if (type == PLIST_ARRAY) {
				plist_array_remove_item(parent, plist_array_get_item_index(node));
			}
			else if (type == PLIST_DICT) {
				char* key = NULL;
				plist_dict_get_item_key(node, &key);
				plist_dict_remove_item(parent, key);
				free(key);
			}

			//update tree model
			gtk_tree_store_remove(app.document_tree_store, &iter);
		}
	}
}


void type_edited_cb(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, gpointer user_data)
{
	plist_t node;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	plist_type type = PLIST_NONE;
	GtkTreeModel *model = GTK_TREE_MODEL(app.document_tree_store);

	gtk_tree_model_get_iter_from_string(model, &iter, path_string);
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get(model, &iter, 0, &node, -1);

	if (!strcmp(new_text,"Boolean")){
		type = PLIST_BOOLEAN;
	}
	else if (!strcmp(new_text,"Number")){
		type = PLIST_UINT;
	}
	else if (!strcmp(new_text,"Float")){
		type = PLIST_REAL;
	}
	else if (!strcmp(new_text,"String")){
		type = PLIST_STRING;
	}
	else if (!strcmp(new_text,"Data")){
		type = PLIST_DATA;
	}
	else if (!strcmp(new_text,"Date")){
		type = PLIST_DATE;
	}
	else if (!strcmp(new_text,"Array")){
		type = PLIST_ARRAY;
	}
	else if (!strcmp(new_text,"Dictionary")){
		type = PLIST_DICT;
	}
	/* plist_set_type(node, type); Removed from libplist on 20/05/14 with Removed plist_set_type() as it should not be used.*/
	gtk_tree_model_row_changed(model, path, &iter);
}

void key_edited_cb(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, gpointer user_data)
{
	plist_t node;
	plist_t father;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	plist_type type = PLIST_NONE;
	GtkTreeModel *model = GTK_TREE_MODEL(app.document_tree_store);

	gtk_tree_model_get_iter_from_string(model, &iter, path_string);
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get(model, &iter, 0, &node, -1);

	father = plist_get_parent(node);
	if (father){
		type = plist_get_node_type(father);
		if (PLIST_DICT == type){
			char* old_key = NULL;
			plist_t backup = plist_copy(node);
			plist_dict_get_item_key(node, &old_key);
			plist_dict_remove_item(father, old_key);
			free(old_key);
			plist_dict_set_item(father, new_text, backup);
			gtk_tree_store_set(app.document_tree_store,
					   &iter,
					   0,
					   (gpointer)backup,
					   -1);
		}
	}
	gtk_tree_model_row_changed(model, path, &iter);
}

void value_edited_cb(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, gpointer user_data)
{
	plist_t node;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	plist_type type = PLIST_NONE;
	GtkTreeModel *model = GTK_TREE_MODEL(app.document_tree_store);

	double d;
	uint8_t b;
	uint64_t u;

	gtk_tree_model_get_iter_from_string(model, &iter, path_string);
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get(model, &iter, 0, &node, -1);

	type = plist_get_node_type(node);
	switch (type){
	case PLIST_BOOLEAN:
		b = !strcmp(new_text, "true") ? 1 : 0;
		plist_set_bool_val(node, b);
		break;
	case PLIST_UINT:
		u = g_ascii_strtoull(new_text, NULL, 0);
		plist_set_uint_val(node, u);
		break;
	case PLIST_REAL:
		d = g_ascii_strtod(new_text, NULL);
		plist_set_real_val(node, d);
		break;
	case PLIST_STRING:
		plist_set_string_val(node, new_text);
		break;
	case PLIST_DATA:
		//TODO
		break;
	case PLIST_DATE:
		//TODO
		break;
	case PLIST_ARRAY:
	case PLIST_DICT:
		break;
	default:
		break;
	}
	gtk_tree_model_row_changed(model, path, &iter);
}


void about_menu_item_activate_cb(GtkMenuItem *item, gpointer user_data) {

        const gchar *authors[] = {
            "Martin Szulecki <opensuse@sukimashita.com>",
            "Jonathan Beck <jonabeck@gmail.com>",
	    "Timothy Ward <gtwa001@gmail.com>",NULL
        };

        gtk_show_about_dialog(app.main_window,

        "name", "Gnome Property List Editor",
        "version", PACKAGE_VERSION,
        "comments", "A developer's Property List Editor",
        "authors", authors,
        "logo-icon-name", "text-editor",
        "website", "http://cgit.sukimashita.com/gnome-plist-editor.git/",
        "website-label", "GIT Source Code Repository",
        "license-type", GTK_LICENSE_LGPL_2_1,
        "wrap-license", "TRUE",
        NULL);
}

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
	GTimeVal val = { 0, 0 };

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
			plist_get_date_val(node, (int32_t*)&val.tv_sec, (int32_t*)&val.tv_usec);
			text = g_time_val_to_iso8601(&val);
			break;
		case PLIST_ARRAY:
			text = g_strdup_printf("(%d items)", plist_array_get_size(node));
			g_object_set(renderer, "sensitive", FALSE, NULL);
			break;
		case PLIST_DICT:
			text = g_strdup_printf("(%d items)", plist_dict_get_size(node));
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
	GtkTreeSelection *selection;

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

	/* selection mode */
	selection = gtk_tree_view_get_selection(app.document_tree_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
}

int main(int argc, char **argv) {
	GtkBuilder *gtk_builder;

	/* gtk_set_locale(); This has been deprecated in gtk3 use setlocale() */
    setlocale();

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


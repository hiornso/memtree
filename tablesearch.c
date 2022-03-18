#include <gtk/gtk.h>

#include "tablesearch.h"

static gboolean recursive_search(GtkTreeModel *model,
								 int column,
								 const char *key,
								 GtkTreeIter *iter,
								 gpointer search_data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(search_data);
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);
	
	gchararray str;
	gtk_tree_model_get(model, iter, column, &str, -1);
	if (strncasecmp(str, key, strlen(key)) == 0) {
		g_free(str);
		GtkTreePath *parentpath = gtk_tree_path_copy(path);
		if (gtk_tree_path_up(parentpath)) { // if there was a parent - if not, this was toplevel, so must be shown anyway
			gtk_tree_view_expand_to_path(treeview, parentpath);
		}
		gtk_tree_path_free(parentpath);
		gtk_tree_view_scroll_to_cell(treeview, path, NULL, FALSE, 0.0f, 0.0f);
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
		return FALSE;
	}
	g_free(str);
	
	GtkTreeIter child;
	if (gtk_tree_model_iter_children(model, &child, iter)) {
		do {
			gboolean no_match = recursive_search(model, column, key, &child, search_data);
			if (!no_match) {
				gtk_tree_path_free(path);
				return FALSE;
			}
		} while (gtk_tree_model_iter_next(model, &child));
	}
	
	gtk_tree_path_free(path);
	return TRUE;
}

void handle_search_changed(GtkSearchEntry *search_entry, gpointer user_data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(user_data);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int column = gtk_tree_view_get_search_column(treeview);
	const char *key = gtk_editable_get_text(GTK_EDITABLE(search_entry));
	if (*key == '\0') return;
	
	GtkTreeIter child;
	if (gtk_tree_model_get_iter_first(model, &child)) {
		do {
			gboolean no_match = recursive_search(model, column, key, &child, user_data);
			if (!no_match) {
				return;
			}
		} while (gtk_tree_model_iter_next(model, &child));
	}
}

#include <gtk/gtk.h>
#include <stddef.h>

#include "tablesort.h"

enum col_indices {
	PROCNAME_COLUMN,
	EXECUTABLE_COLUMN,
	
	PID_COLUMN,
	
	USS_COLUMN,
	PSS_COLUMN,
	RSS_COLUMN,
	
	SWAP_COLUMN,
	SWAP_PSS_COLUMN,
	
	TOTAL_USS_COLUMN,
	TOTAL_PSS_COLUMN,
};

static gint compare_iters(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	const int col = (int)(intptr_t)user_data;
	
	if (col == PROCNAME_COLUMN || col == EXECUTABLE_COLUMN) {
		gchar *str_a, *str_b;
		gtk_tree_model_get(model, a, col, &str_a, -1);
		gtk_tree_model_get(model, b, col, &str_b, -1);
		
		if (str_a == NULL) {
			if (str_b == NULL) return 0;
			else return -1;
		} else { // str_a is non-NULL
			if (str_b == NULL) return 1;
			else return strcmp(str_a, str_b);
		}
	} else if (col == PID_COLUMN) {
		gint int_a, int_b;
		
		gtk_tree_model_get(model, a, col, &int_a, -1);
		gtk_tree_model_get(model, b, col, &int_b, -1);
		
		return int_a - int_b;
	} else {
		glong long_a, long_b;
		
		gtk_tree_model_get(model, a, col, &long_a, -1);
		gtk_tree_model_get(model, b, col, &long_b, -1);
		
		return long_a - long_b;
	}
}

void set_sort_funcs(GtkTreeStore *treestore)
{
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(treestore);
	
	gtk_tree_sortable_set_sort_func(sortable, PROCNAME_COLUMN, compare_iters, (gpointer)PROCNAME_COLUMN, NULL);
	gtk_tree_sortable_set_sort_func(sortable, EXECUTABLE_COLUMN, compare_iters, (gpointer)EXECUTABLE_COLUMN, NULL);
	
	gtk_tree_sortable_set_sort_func(sortable, PID_COLUMN, compare_iters, (gpointer)PID_COLUMN, NULL);
	
	gtk_tree_sortable_set_sort_func(sortable, USS_COLUMN, compare_iters, (gpointer)USS_COLUMN, NULL);
	gtk_tree_sortable_set_sort_func(sortable, PSS_COLUMN, compare_iters, (gpointer)PSS_COLUMN, NULL);
	gtk_tree_sortable_set_sort_func(sortable, RSS_COLUMN, compare_iters, (gpointer)RSS_COLUMN, NULL);
	
	gtk_tree_sortable_set_sort_func(sortable, SWAP_COLUMN, compare_iters, (gpointer)SWAP_COLUMN, NULL);
	gtk_tree_sortable_set_sort_func(sortable, SWAP_PSS_COLUMN, compare_iters, (gpointer)SWAP_PSS_COLUMN, NULL);
	
	gtk_tree_sortable_set_sort_func(sortable, TOTAL_USS_COLUMN, compare_iters, (gpointer)TOTAL_USS_COLUMN, NULL);
	gtk_tree_sortable_set_sort_func(sortable, TOTAL_PSS_COLUMN, compare_iters, (gpointer)TOTAL_PSS_COLUMN, NULL);
	
	gtk_tree_sortable_set_sort_column_id(sortable, PID_COLUMN, GTK_SORT_ASCENDING);
}

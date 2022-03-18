#ifndef _MEMTREE_GETAPPS_H
#define _MEMTREE_GETAPPS_H

#include <stddef.h>
#include <gtk/gtk.h>

typedef struct app_list {
	size_t n;
	char **app_names;
	char **app_executables;
	gchar **app_icons_as_string;
} AppList;

AppList get_apps();
void free_app_list(AppList app_list);

#endif

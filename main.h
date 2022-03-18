#ifndef _MEMTREE_MAIN_H
#define _MEMTREE_MAIN_H

#include <gtk/gtk.h>
#include <sys/time.h>

#include "backend.h"

typedef struct cmd_line_options {
	gboolean print_version;
} CmdLineOptions;

typedef struct refresh_widgets {
	GtkTreeStore *treestore;
	GtkLevelBar *mem_levelbar, *swap_levelbar;
	GtkLabel *mem_label, *swap_label;
	struct timeval last_refresh;
	PInfo *last_pinfos;
} RefreshWidgets;

#endif

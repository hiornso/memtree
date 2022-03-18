#ifndef _MEMTREE_TABLE_H
#define _MEMTREE_TABLE_H

#include "backend.h"

PInfo* init_table(GtkTreeStore *treestore);
PInfo* refresh_table(GtkTreeStore *treestore, PInfo *old_pinfos);

#endif

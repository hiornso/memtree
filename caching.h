#ifndef _MEMTREE_CACHING_H
#define _MEMTREE_CACHING_H

#include "backend.h"

PInfo* parse_proc_get_cached(size_t *len);
PInfo* cache_proc(PInfo *pinfos, size_t len);
int queue_cache_update();
void deinit_cache();

#endif

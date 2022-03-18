#ifndef _MEMTREE_BACKEND_H
#define _MEMTREE_BACKEND_H

#include <sys/types.h>
#include <stdbool.h>
#include <gtk/gtk.h>

typedef struct system_meminfo {
	bool valid;
	long mem_total, mem_available, swap_total, swap_free;
} SystemMemInfo;

typedef struct meminfo {
	long uss, pss, rss, swap, swap_pss, est_total_uss, total_pss;
} MemInfo;

typedef struct process_info {
	bool valid, no_mem_info;
	char *procname;
	char *executable;
	pid_t pid;
	MemInfo mem, mem_recursive;
	pid_t parent;
	GtkTreeIter iter;
} PInfo;

void free_parsed(PInfo *pinfos);
PInfo* copy_parsed(PInfo *pinfos, const size_t len);
long find_pid(PInfo *pinfos, const pid_t pid, const size_t len);
PInfo *parse_proc(size_t *len);
SystemMemInfo get_system_memory_info();

#endif

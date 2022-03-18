#include <gtk/gtk.h>
#include <assert.h>

#include "table.h"
#include "backend.h"
#include "caching.h"

#define DISABLE_PROCESS_LIST_LOG_PRINTF 1

#if NDEBUG || DISABLE_PROCESS_LIST_LOG_PRINTF
#define cprintf_process_list(...)
#else
#define cprintf_process_list(...) printf(__VA_ARGS__)
#endif

static void update_process_memory_values(GtkTreeStore *treestore, PInfo *pinfo)
{
	MemInfo info = pinfo->mem_recursive; // TODO: add checkbox / preference for recursive vs only self 
	gtk_tree_store_set(treestore, &pinfo->iter,
		3, info.uss,
		4, info.pss,
		5, info.rss,
	
		6, info.swap,
		7, info.swap_pss,
	
		8, info.est_total_uss,
		9, info.total_pss,
	-1);
}

static void add_process_to_treestore(GtkTreeStore *treestore, PInfo *pinfo, PInfo *pinfos, const size_t len)
{
	if (pinfo->parent == 0) {
		gtk_tree_store_append(treestore, &pinfo->iter, NULL);
	} else {
		size_t parent_index = find_pid(pinfos, pinfo->parent, len);
		assert(parent_index >= 0);
		gtk_tree_store_append(treestore, &pinfo->iter, &pinfos[parent_index].iter);
	}
	
	if (pinfo->procname != NULL) gtk_tree_store_set(treestore, &pinfo->iter, 0, pinfo->procname, -1);
	if (pinfo->executable != NULL) gtk_tree_store_set(treestore, &pinfo->iter, 1, pinfo->executable, -1);
	gtk_tree_store_set(treestore, &pinfo->iter, 2, pinfo->pid, -1);
	
	update_process_memory_values(treestore, pinfo);
}

PInfo* init_table(GtkTreeStore *treestore)
{
	gtk_tree_store_clear(treestore);
	
	size_t len;
	PInfo *pinfos = parse_proc(&len);
	if (pinfos == NULL) g_error("`parse_proc()` returned NULL");
	PInfo *cached = cache_proc(pinfos, len);
	if (cached == NULL) g_critical("`cache_proc()` returned NULL");
	
	for (int i = 0; pinfos[i].pid != 0; ++i) {
		if (!pinfos[i].valid) continue;
		
		add_process_to_treestore(treestore, &pinfos[i], pinfos, len);
	}
	
	return pinfos;
}

PInfo* refresh_table(GtkTreeStore *treestore, PInfo *old_pinfos)
{
	size_t len;
	PInfo *const pinfos = parse_proc_get_cached(&len);
	if (pinfos == NULL) {
		g_critical("`parse_proc_get_cached()` returned NULL, skipping this table refresh");
		return old_pinfos;
	}
	
	PInfo *end_of_old;
	PInfo *pinfo = old_pinfos;
	for (; pinfo->pid != 0; ++pinfo); // move pinfo to the null PInfo at the end of old_pinfos
	end_of_old = pinfo - 1; // 1 before the null PInfo
	while (!end_of_old->valid) {
		--end_of_old;
	}
	
	PInfo *end_of_old_in_new = pinfos;
	while (1) {
		PInfo *next = end_of_old_in_new + 1;
		if (next->pid == 0 || next->pid > end_of_old->pid) break;
		++end_of_old_in_new; // move this to the final PInfo in the new pinfos with a pid <= the max pid in old_pinfos
	}
	while (!end_of_old_in_new->valid) {
		--end_of_old_in_new;
	}
	
	for (PInfo *pinfo = end_of_old_in_new; pinfo >= pinfos; --pinfo) { // going backwards through new pinfos to see what's been removed
		if (!pinfo->valid) continue;
		// copy across iters and remove processes which no longer exist from the tree
		assert(pinfo->pid > 0);
		while (end_of_old->pid > pinfo->pid) { // bring the pointer into old pids back to where we are - any processes it passes over are missing from the new pinfos list
			if (!end_of_old->valid) { // was never added to tree
				--end_of_old;
				continue;
			}
			
			cprintf_process_list("process %i (%s) died\n", end_of_old->pid, end_of_old->procname);
			
			// remove process with pid end_of_old->pid from tree
			gtk_tree_store_remove(treestore, &end_of_old->iter);
			
			--end_of_old;
		}
		
		assert(end_of_old->pid == pinfo->pid);
		
		// each run of this is done for what is in common between the trees - so we use this to copy across iters for each existing tree element and update memory info
		pinfo->iter = end_of_old->iter;
		update_process_memory_values(treestore, pinfo);
		
		--end_of_old;
	}
	
	for (PInfo *pinfo = end_of_old_in_new + 1; pinfo->pid != 0; ++pinfo) { // going forwards through newly created processes
		if (!pinfo->valid) continue;
		
		// add new processes
		cprintf_process_list("new process %i (%s)\n", pinfo->pid, pinfo->procname);
		
		add_process_to_treestore(treestore, pinfo, pinfos, len);
	}
	
	free_parsed(old_pinfos);
	
	return pinfos;
}

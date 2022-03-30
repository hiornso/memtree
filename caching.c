#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "caching.h"
#include "backend.h"

static bool first = true;
static pthread_t tid;
static bool thread_done = false;
static PInfo *thread_pinfos = NULL;
static size_t thread_len = 0;

static PInfo *cached_pinfos = NULL;
static size_t cached_len = 0;

PInfo *parse_proc_get_cached(size_t *len)
{
	atomic_thread_fence(memory_order_acquire); // get updates from worker thread
	if (thread_done) { // finished generating
		if (cached_pinfos != thread_pinfos) { // but haven't claimed the results
			free_parsed(cached_pinfos);
			
			cached_pinfos = thread_pinfos;
			cached_len = thread_len;
		}
	}
	
	if (len != NULL) {
		*len = cached_len;
	}
	
	return copy_parsed(cached_pinfos, cached_len);
}

PInfo* cache_proc(PInfo *pinfos, size_t len)
{ // makes a copy of pinfos - the caller still owns and is responsible for freeing the pinfos they passed in
	free_parsed(cached_pinfos);
	cached_pinfos = copy_parsed(pinfos, len);
	cached_len = len;
	return cached_pinfos;
}

static void* thread_parse_proc(__attribute__((unused)) void *data)
{
	atomic_thread_fence(memory_order_acquire); // send updates back to main thread
	thread_pinfos = parse_proc(&thread_len);
	thread_done = true;
	atomic_thread_fence(memory_order_release); // send updates back to main thread
	return NULL;
}

int queue_cache_update()
{	
	if (!first) {
		atomic_thread_fence(memory_order_acquire); // get updates from worker thread
		if (thread_done) {
			pthread_join(tid, NULL);
			
			if (cached_pinfos != thread_pinfos) {
				free_parsed(cached_pinfos);
				cached_pinfos = thread_pinfos;
				cached_len = thread_len;
			}
		} else {
			return -1;
		}
	} else {
		first = false;
	}
	
	thread_done = false;
	pthread_create(&tid, NULL, thread_parse_proc, NULL);
	
	return 0;
}

void deinit_cache() // to be called only when the cache will never be used again, at program exit
{
	free_parsed(cached_pinfos);
	
	atomic_thread_fence(memory_order_acquire); // get updates from worker thread
	if (!first && !thread_done) {
		pthread_join(tid, NULL);
		
		free_parsed(thread_pinfos);
	}
}

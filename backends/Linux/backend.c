#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "backend.h"

#define DISABLE_BACKEND_PRINTF 1

#if NDEBUG || DISABLE_BACKEND_PRINTF
#define cprintf(...)
#else
#define cprintf(...) printf(__VA_ARGS__)
#endif

static int linux_major_version = -1;

int init_backend() {
	struct utsname sys_info;
	if (uname(&sys_info) != 0) {
		printf("Error: failed to get system info with uname syscall. This means the backend cannot be initialised. Exiting...\n");
		return -1;
	}
	
	if (strcmp(sys_info.sysname, "Linux") != 0) {
		printf("Error: this program was compiled for Linux, but is not being run on Linux! As such, the backend cannot be initialised. Exiting...\n");
		return -2;
	}
	
	int linux_minor_version, linux_micro_version;
	sscanf(sys_info.release, "%i.%i.%i", &linux_major_version, &linux_minor_version, &linux_micro_version);
	printf("Using Linux backend, on Linux v%i.%i.%i. (Release %s)\n", linux_major_version, linux_minor_version, linux_micro_version, sys_info.release);
	
	return 0;
}

void free_parsed(PInfo *pinfos)
{
	if (pinfos == NULL) return;
	for (int i = 0; pinfos[i].pid != 0; ++i) {
		free(pinfos[i].executable);
		free(pinfos[i].procname);
	}
	free(pinfos);
}

PInfo* copy_parsed(PInfo *pinfos, const size_t len)
{
	PInfo *new_pinfos = malloc((len + 1) * sizeof(PInfo));
	memcpy(new_pinfos, pinfos, (len + 1) * sizeof(PInfo));
	
	for (size_t i = 0; i < len; ++i) {
		if (new_pinfos[i].executable != NULL) new_pinfos[i].executable = strdup(new_pinfos[i].executable);
		if (new_pinfos[i].procname   != NULL) new_pinfos[i].procname   = strdup(new_pinfos[i].procname);
		
		if ((new_pinfos[i].executable == NULL && pinfos[i].executable != NULL) || 
			(new_pinfos[i].procname   == NULL && pinfos[i].procname   != NULL)) {
			for (size_t j = 0; j <= i; ++j) {
				free(new_pinfos[j].executable); // free duplicates
				free(new_pinfos[j].procname);
			}
			
			free(new_pinfos);
			
			return NULL;
		}
	}
	
	return new_pinfos;
}

static int is_num(const struct dirent *dir)
{
	for (const char *p = dir->d_name; *p != '\0'; ++p) {
		if (!('0' <= *p && *p <= '9')) {
			return 0; // not purely numeric
		} 
	}
	return 1; // purely numeric
}

static int sort_as_nums(const struct dirent **a, const struct dirent **b)
{
	return atol((*a)->d_name) > atol((*b)->d_name);
}

static void skipline(FILE *f)
{
	char c;
	while ((c = fgetc(f))) {
		if (c == '\n' || c == EOF) return;
	}
}

long find_pid(PInfo *pinfos, const pid_t pid, const size_t len)
{
	size_t bounds[2] = {0, len - 1};
	while (1) {
		size_t try = (bounds[0] + bounds[1]) / 2;
		pid_t try_pid = pinfos[try].pid;
		
		if (try_pid == pid) return try;
		else if (try_pid > pid) {
			bounds[1] = try - 1;
		} else { // try_pid < pid
			if (bounds[1] == bounds[0]) {
				return -1;
			}
			bounds[0] = try + 1;
		}
	}
}

PInfo *parse_proc(size_t *len)
{
	const char procpath[] = "/proc";
	const size_t procpath_len = strlen(procpath);
	char path[32];
	assert(procpath_len + 1 + 1 < sizeof(path)); //   "/proc" + "/" + '\0'
	strcpy(path, procpath);
	path[procpath_len] = '/';
	char * const subpath = path + procpath_len + 1;
	
	struct dirent **dirs = NULL;
	const int n_dirs = scandir(procpath, &dirs, is_num, sort_as_nums);
	if (n_dirs < 1) {
		g_critical("error in `scandir`: %s", strerror(errno));
		return NULL;
	}
	assert(n_dirs >= 0);
	
	PInfo *ret = calloc(n_dirs + 1, sizeof(PInfo));
	if (ret == NULL) return NULL;
	ret[n_dirs].pid = 0;
	
	for (int i = 0; i < n_dirs; ++i) {
		cprintf("%3i/%3i \t%7s \t", i, n_dirs, dirs[i]->d_name);
		ret[i].pid = atoi(dirs[i]->d_name);
		ret[i].valid = true;
		
		const size_t len = strlen(dirs[i]->d_name);
		assert(procpath_len + 1 + len + 1 + 1 < sizeof(path)); //   "/proc" + "/" + "1234567" + "/" + '\0'
		
		strcpy(subpath, dirs[i]->d_name);
		subpath[len] = '/';
		
		char * const filename_p = subpath + len + 1;
		
		int comm_len;
		{
			const char filename[] = "comm";
			assert(procpath_len + 1 + len + 1 + strlen(filename) < sizeof(path)); //   "/proc" + "/" + "1234567" + "/" + "status" + '\0'
			strcpy(filename_p, filename);
			
			FILE *f = fopen(path, "rb");
			
			if (f != NULL) {
				comm_len = 0;
				while (fgetc(f) != EOF) comm_len++;
				// null char will overwrite newline included in file
				
				char *s = malloc(comm_len);
				if (s != NULL) {
					fseek(f, 0, SEEK_SET);
					fread(s, comm_len - 1, 1, f);
					s[comm_len - 1] = '\0';
					cprintf("%-32s\t", s);
				}
				
				ret[i].procname = s;
				
				fclose(f);
			} else {
				ret[i].valid = false;
				free(dirs[i]);
				cprintf("\n");
				continue;
			}
		}
		
		{
			const char filename[] = "stat";
			assert(procpath_len + 1 + len + 1 + strlen(filename) < sizeof(path)); //   "/proc" + "/" + "1234567" + "/" + "status" + '\0'
			strcpy(filename_p, filename);
			
			FILE *f = fopen(path, "rb");
			
			if (f != NULL) {
				fscanf(f, "%*i (");
				fseek(f, comm_len - 1, SEEK_CUR); // -1 because we don't want to count the newline
				fscanf(f, ") %*s %i", &ret[i].parent);
				
				cprintf("%7i \t", ret[i].parent);
				
				fclose(f);
			} else {
				ret[i].valid = false;
				free(dirs[i]);
				free(ret[i].procname);
				ret[i].procname = NULL;
				cprintf("\n");
				continue;
			}
		}
		
		{
			const char filename[] = "smaps_rollup";
			assert(procpath_len + 1 + len + 1 + strlen(filename) < sizeof(path)); //   "/proc" + "/" + "1234567" + "/" + "status" + '\0'
			strcpy(filename_p, filename);
			
			FILE *f = fopen(path, "rb");
			
			if (f != NULL) {
				long int uss, rss, pss, private_clean, private_dirty, swap, swap_pss;
				
				if (getc(f) == EOF) {
					ret[i].no_mem_info = true;
					fclose(f);
				} else {
					// TODO: make this seek-based rather than skip-based, so that it does not break when new fields are added to this file
					skipline(f); // 1234000-5678000 ---p 00000000 00:00 0
					fscanf(f, "Rss: %li kB\n", &rss);
					fscanf(f, "Pss: %li kB\n", &pss);
					if (linux_major_version >= 6) skipline(f); // Pss_Dirty (new in linux 6.0)
					skipline(f); // Pss_Anon
					skipline(f); // Pss_File
					skipline(f); // Pss_Shmem
					skipline(f); // Shared_Clean
					skipline(f); // Shared_Dirty
					fscanf(f, "Private_Clean: %li kB\n", &private_clean);
					fscanf(f, "Private_Dirty: %li kB\n", &private_dirty);
					fscanf(f, "Private_Clean: %li kB\n", &private_clean);
					skipline(f); // Referenced
					skipline(f); // Anonymous
					skipline(f); // LazyFree
					skipline(f); // AnonHugePages
					skipline(f); // ShmemPmdMapped
					skipline(f); // FilePmdMapped
					skipline(f); // Shared_Hugetlb
					skipline(f); // Private_Hugetlb
					fscanf(f, "Swap: %li kB\n", &swap);
					fscanf(f, "SwapPss: %li kB\n", &swap_pss);
					skipline(f); // Locked
					
					fclose(f);
					
					uss = private_clean + private_dirty;
					
					ret[i].mem = (MemInfo){
				// long uss, pss, rss, swap, swap_pss, est_total_uss,  total_pss;
						uss, pss, rss, swap, swap_pss, uss + swap_pss, pss + swap_pss
					};
					ret[i].mem_recursive = ret[i].mem;
					
					cprintf("mem={%5li, %5li, %5li, %5li, %5li, ...}  ", uss, pss, rss, swap, swap_pss);
				}
				
			} else if (errno == EACCES) {
				ret[i].no_mem_info = true;
			} else {
				ret[i].valid = false;
				free(dirs[i]);
				free(ret[i].procname);
				ret[i].procname = NULL;
				cprintf("\n");
				continue;
			}
		}
		
		{
			const char filename[] = "exe";
			assert(procpath_len + 1 + len + 1 + strlen(filename) + 1 < sizeof(path)); //   "/proc" + "/" + "1234567" + "/" + "status" + '\0'
			strcpy(filename_p, filename);
			
			int bufsize = 32, written;
			char *buf;
			while (1) {
				buf = malloc(bufsize);
				if (buf == NULL) break;
				
				written = readlink(path, buf, bufsize);
				if (written < 0) {
					free(buf);
					buf = NULL;
					break;
				}
				if (written < bufsize) {
					buf[written] = '\0';
					break;
				}
				free(buf);
				bufsize += 32;
			}
			
			ret[i].executable = buf;
			
			if (buf) cprintf("%s", buf);
			cprintf("\n");
		}
		
		free(dirs[i]);
	}
	
	for (int i = n_dirs - 1; i >= 0; --i) {
		if (ret[i].parent > 0) {
			int j = find_pid(ret, ret[i].parent, n_dirs);
			if (j >= 0) {
				ret[j].mem_recursive.uss           += ret[i].mem_recursive.uss;
				ret[j].mem_recursive.pss           += ret[i].mem_recursive.pss;
				ret[j].mem_recursive.rss           += ret[i].mem_recursive.rss;
				ret[j].mem_recursive.swap          += ret[i].mem_recursive.swap;
				ret[j].mem_recursive.swap_pss      += ret[i].mem_recursive.swap_pss;
				ret[j].mem_recursive.est_total_uss += ret[i].mem_recursive.est_total_uss;
				ret[j].mem_recursive.total_pss     += ret[i].mem_recursive.total_pss;
			}
		}
	}
	
	free(dirs);
	
	assert(ret[n_dirs].pid == 0);
	
	if (len != NULL) {
		*len = n_dirs;
	}
	
	return ret;
}

SystemMemInfo get_system_memory_info()
{
	SystemMemInfo ret = (SystemMemInfo){true, 0,0,0,0};
	
	FILE *f = fopen("/proc/meminfo", "rb");
	if (f == NULL) {
		ret.valid = false;
		return ret;
	}
	
	// NB: even though the file says "kB" it's actually in KiB. They won't fix it for backwards compatibility reasons.
	fscanf(f, "MemTotal: %li kB\n", &ret.mem_total);
	skipline(f); // MemFree
	fscanf(f, "MemAvailable: %li kB\n", &ret.mem_available);
	skipline(f); // Buffers
	skipline(f); // Cached
	skipline(f); // SwapCached
	skipline(f); // Active
	skipline(f); // Inactive
	skipline(f); // Active(anon)
	skipline(f); // Inactive(anon)
	skipline(f); // Active(file)
	skipline(f); // Inactive(file)
	skipline(f); // Unevictable
	skipline(f); // Mlocked
	fscanf(f, "SwapTotal: %li kB\n", &ret.swap_total);
	fscanf(f, "SwapFree: %li kB\n", &ret.swap_free);
	
	fclose(f);
	
	cprintf("Memory: %li/%li KiB\tSwap: %li/%li KiB\n", ret.mem_total - ret.mem_available, ret.mem_total, ret.swap_total - ret.swap_free, ret.swap_total);
	
	return ret;
}

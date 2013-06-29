#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>

#include "dpth.h"
#include "parse.h"
#include "prepend.h"
#include "handy.h"
#include "hash.h"

#define MAX_STORAGE_SUBDIRS	30000

static char *dpth_mk_prim(struct dpth *dpth)
{
	static char path[8];
	snprintf(path, sizeof(path), "%04X", dpth->prim);
	return path;
}

static char *dpth_mk_seco(struct dpth *dpth)
{
	static char path[16];
	snprintf(path, sizeof(path), "%04X/%04X", dpth->prim, dpth->seco);
	return path;
}

static char *dpth_mk_tert(struct dpth *dpth)
{
	static char path[24];
	snprintf(path, sizeof(path), "%04X/%04X/%04X",
		dpth->prim, dpth->seco, dpth->tert);
	return path;
}

char *dpth_mk(struct dpth *dpth)
{
	static char path[32];
	snprintf(path, sizeof(path), "%04X/%04X/%04X/%04X",
		dpth->prim, dpth->seco, dpth->tert, dpth->sig);
	return path;
}

static int process_sig(char cmd, const char *buf, unsigned int s, struct dpth *dpth, void *ignored)
{
        static uint64_t weakint;
        static struct weak_entry *weak_entry;
        static char weak[16+1];
        static char strong[32+1];

        if(split_sig(buf, s, weak, strong)) return -1;

        weakint=strtoull(weak, 0, 16);

	weak_entry=find_weak_entry(weakint);

	// Add to hash table.
	if(!weak_entry && !(weak_entry=add_weak_entry(weakint)))
		return -1;
	if(!(weak_entry->strong=add_strong_entry(weak_entry, strong, dpth)))
		return -1;
	dpth->sig++;

	return 0;
}

// Returns 0 on OK, -1 on error. *max gets set to the next entry.
static int get_highest_entry(const char *path, int *max, struct dpth *dpth)
{
	int ent=0;
	int ret=0;
	DIR *d=NULL;
	char *tmp=NULL;
	struct dirent *dp=NULL;
	FILE *ifp=NULL;

	*max=-1;
	if(!(d=opendir(path))) goto end;
	while((dp=readdir(d)))
	{
		if(dp->d_ino==0
		  || !strcmp(dp->d_name, ".")
		  || !strcmp(dp->d_name, ".."))
			continue;
		ent=strtol(dp->d_name, NULL, 16);
		if(ent>*max) *max=ent;
		if(dpth)
		{
			dpth->tert=ent;
			dpth->sig=0;
			if(!(tmp=prepend_s(path,
				dp->d_name, strlen(dp->d_name))))
					goto error;
			if(!(ifp=file_open(tmp, "rb")))
				goto error;
fprintf(stderr, "LOAD: %s\n", tmp);
			if(split_stream(ifp, dpth, NULL,
				NULL, NULL, process_sig))
					goto error;
			free(tmp); tmp=NULL;
			fclose(ifp); ifp=NULL;
		}
	}

	goto end;
error:
	ret=-1;
end:
	if(d) closedir(d);
	if(ifp) fclose(ifp);
	if(tmp) free(tmp);
	return ret;
}

static int get_next_entry(const char *path, int *max, struct dpth *dpth)
{
	if(get_highest_entry(path, max, dpth)) return -1;
	(*max)++;
	return 0;
}

// Three levels with 65535 entries each gives
// 65535^3 = 281,462,092,005,375 data entries
// recommend a filesystem with lots of inodes?
// Hmm, but ext3 only allows 32000 subdirs, although that many files are OK.
static int dpth_incr(struct dpth *dpth)
{
	if(dpth->tert++<0xFFFF) return 0;
	dpth->tert=0;
	if(dpth->seco++<MAX_STORAGE_SUBDIRS) return 0;
	dpth->seco=0;
	if(dpth->prim++<MAX_STORAGE_SUBDIRS) return 0;
	dpth->prim=0;
	fprintf(stderr, "Could not find any free data file entries out of the 15000*%d*%d available!\n", MAX_STORAGE_SUBDIRS, MAX_STORAGE_SUBDIRS);
	fprintf(stderr, "Recommend moving the client storage directory aside and starting again.\n");
	return -1;
}

static char *dpth_get_path_dat(struct dpth *dpth)
{
	char *ret;
	char *path=dpth_mk_tert(dpth);
	if(!(ret=prepend_s(dpth->base_path_dat, path, strlen(path))))
                fprintf(stderr, "Out of memory in %s.\n", __FUNCTION__);
	return ret;
}

static char *dpth_get_path_man(struct dpth *dpth)
{
	int high;
	char *ret;
	char tmp[16];

	if(get_next_entry(dpth->base_path_man, &high, NULL))
		return NULL;
	snprintf(tmp, sizeof(tmp), "%d", high);
	if(!(ret=prepend_s(dpth->base_path_man, tmp, strlen(tmp))))
                fprintf(stderr, "Out of memory in %s.\n", __FUNCTION__);
	return ret;
}

static char *dpth_get_path_sig(struct dpth *dpth)
{
	char *ret;
	char *path=dpth_mk_tert(dpth);
	if(!(ret=prepend_s(dpth->base_path_sig, path, strlen(path))))
                fprintf(stderr, "Out of memory in %s.\n", __FUNCTION__);
	return ret;
}

int dpth_incr_sig(struct dpth *dpth)
{
	if(++(dpth->sig)<SIG_MAX) return 0;
	dpth->sig=0;

	// Do not need to close dpth->mfp and open a new one, because there is
	// only one manifest per backup.
	if(file_close(&(dpth->dfp))
	  || file_close(&(dpth->sfp)))
		return -1;
	if(dpth_incr(dpth)) return -1;
	if(dpth->path_dat) free(dpth->path_dat);
	if(dpth->path_sig) free(dpth->path_sig);

	// Should do the open here too, instead of in parse.c.
	dpth->path_dat=dpth_get_path_dat(dpth);
	dpth->path_sig=dpth_get_path_sig(dpth);
	return 0;
}

struct dpth *dpth_alloc(const char *base_path)
{
        struct dpth *dpth;
        if(!(dpth=calloc(1, sizeof(struct dpth)))
	  || !(dpth->base_path=strdup(base_path))
	  || !(dpth->base_path_dat=prepend_s(base_path, "dat", strlen("dat")))
	  || !(dpth->base_path_man=prepend_s(base_path, "man", strlen("man")))
	  || !(dpth->base_path_sig=prepend_s(base_path, "sig", strlen("sig"))))
        {
                fprintf(stderr, "Out of memory in %s.\n", __FUNCTION__);
                goto error;
        }
	goto end;
error:
	dpth_free(dpth);
	dpth=NULL;
end:
	return dpth;
}

int dpth_init(struct dpth *dpth)
{
	int max;
	int ret=0;
	char *tmp=NULL;

	if(get_highest_entry(dpth->base_path_sig, &max, NULL))
		goto error;
	if(max<0) max=0;
	dpth->prim=max;
	tmp=dpth_mk_prim(dpth);
	if(!(tmp=prepend_s(dpth->base_path_sig, tmp, strlen(tmp))))
		goto error;

	if(get_highest_entry(tmp, &max, NULL))
		goto error;
	if(max<0) max=0;
	dpth->seco=max;
	free(tmp);
	tmp=dpth_mk_seco(dpth);
	if(!(tmp=prepend_s(dpth->base_path_sig, tmp, strlen(tmp))))
		goto error;

	if(get_next_entry(tmp, &max, dpth))
		goto error;
	if(max<0) max=0;
	dpth->tert=max;

	dpth->sig=0;

	if(!(dpth->path_dat=dpth_get_path_dat(dpth))) goto error;
	if(!(dpth->path_man=dpth_get_path_man(dpth))) goto error;
	if(!(dpth->path_sig=dpth_get_path_sig(dpth))) goto error;

	goto end;
error:
	ret=-1;
end:
	if(tmp) free(tmp);
	return ret;
}

void dpth_free(struct dpth *dpth)
{
	if(!dpth) return;
	if(dpth->base_path) free(dpth->base_path);
	if(dpth->base_path_dat) free(dpth->base_path_dat);
	if(dpth->base_path_man) free(dpth->base_path_man);
	if(dpth->base_path_sig) free(dpth->base_path_sig);
	if(dpth->path_dat) free(dpth->path_dat);
	if(dpth->path_man) free(dpth->path_man);
	if(dpth->path_sig) free(dpth->path_sig);
	if(dpth->dfp) fclose(dpth->dfp);
	if(dpth->mfp) fclose(dpth->mfp);
	if(dpth->sfp) fclose(dpth->sfp);
	free(dpth);
	dpth=NULL;
}
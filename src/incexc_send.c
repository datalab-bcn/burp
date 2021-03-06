#include "include.h"

static int send_incexc_str(struct asfd *asfd, const char *pre, const char *str)
{
	char *tosend=NULL;
	int rc=0;
	if(!str) return 0;
	if(!(tosend=prepend(pre, str, strlen(str), " = ")))
		rc=-1;
	if(!rc && asfd->write_str(asfd, CMD_GEN, tosend))
	{
		logp("Error in async_write_str when sending incexc\n");
		rc=-1;
	}
	if(tosend)
		free(tosend);
	return rc;
}

static int send_incexc_int(struct asfd *asfd, const char *pre, int myint)
{
	char tmp[64]="";
	snprintf(tmp, sizeof(tmp), "%d", myint);
	return send_incexc_str(asfd, pre, tmp);
}

static int send_incexc_long(struct asfd *asfd, const char *pre, long mylong)
{
	char tmp[32]="";
	snprintf(tmp, sizeof(tmp), "%lu", mylong);
	return send_incexc_str(asfd, pre, tmp);
}

static int send_incexc_from_strlist(struct asfd *asfd,
	const char *prepend_on, const char *prepend_off, struct strlist *list)
{
	struct strlist *l;
	for(l=list; l; l=l->next)
		if(send_incexc_str(asfd,
			l->flag?prepend_on:prepend_off, l->path)) return -1;
	return 0;
}

static int do_sends(struct asfd *asfd, struct conf *conf)
{
	if(  send_incexc_from_strlist(asfd, "include", "exclude",
		conf->incexcdir)
	  || send_incexc_from_strlist(asfd, "include_glob", "include_glob",
		conf->incglob)
	  || send_incexc_from_strlist(asfd, "cross_filesystem", "cross_filesystem",
		conf->fschgdir)
	  || send_incexc_from_strlist(asfd, "nobackup", "nobackup",
		conf->nobackup)
	  || send_incexc_from_strlist(asfd, "include_ext", "include_ext",
		conf->incext)
	  || send_incexc_from_strlist(asfd, "exclude_ext", "exclude_ext",
		conf->excext)
	  || send_incexc_from_strlist(asfd, "include_regex", "include_regex",
		conf->increg)
	  || send_incexc_from_strlist(asfd, "exclude_regex", "exclude_regex",
		conf->excreg)
	  || send_incexc_from_strlist(asfd, "exclude_fs", "exclude_fs",
		conf->excfs)
	  || send_incexc_from_strlist(asfd, "exclude_comp", "exclude_comp",
		conf->excom)
	  || send_incexc_from_strlist(asfd, "read_fifo", "read_fifo",
		conf->fifos)
	  || send_incexc_from_strlist(asfd, "read_blockdev", "read_blockdev",
		conf->blockdevs)
	  || send_incexc_int(asfd, "cross_all_filesystems",
		conf->cross_all_filesystems)
	  || send_incexc_int(asfd, "read_all_fifos", conf->read_all_fifos)
	  || send_incexc_long(asfd, "min_file_size", conf->min_file_size)
	  || send_incexc_long(asfd, "max_file_size", conf->max_file_size)
	  || send_incexc_str(asfd, "vss_drives", conf->vss_drives))
		return -1;
	return 0;
}

static int do_sends_restore(struct asfd *asfd, struct conf *conf)
{
	if(  send_incexc_from_strlist(asfd, "include", "exclude", conf->incexcdir)
	  || send_incexc_str(asfd, "orig_client", conf->orig_client)
	  || send_incexc_str(asfd, "backup", conf->backup)
	  || send_incexc_str(asfd, "restoreprefix", conf->restoreprefix)
	  || send_incexc_str(asfd, "regex", conf->regex)
	  || send_incexc_int(asfd, "overwrite", conf->overwrite)
	  || send_incexc_long(asfd, "strip", conf->strip))
		return -1;
	return 0;
}

static int do_request_response(struct asfd *asfd,
	const char *reqstr, const char *repstr)
{
	return (asfd->write_str(asfd, CMD_GEN, reqstr)
	  || asfd->read_expect(asfd, CMD_GEN, repstr));
}

int incexc_send_client(struct asfd *asfd, struct conf *conf)
{
	if(do_request_response(asfd, "incexc", "incexc ok")
	  || do_sends(asfd, conf)
	  || do_request_response(asfd, "incexc end", "incexc end ok"))
		return -1;
	return 0;
}

int incexc_send_server(struct asfd *asfd, struct conf *conf)
{
	/* 'sincexc' and 'sincexc ok' have already been exchanged,
	   so go straight into doing the sends. */
	if(do_sends(asfd, conf)
	  || do_request_response(asfd, "sincexc end", "sincexc end ok"))
		return -1;
	return 0;
}

int incexc_send_server_restore(struct asfd *asfd, struct conf *conf)
{
	/* 'srestore' and 'srestore ok' have already been exchanged,
	   so go straight into doing the sends. */
	if(do_sends_restore(asfd, conf)
	  || do_request_response(asfd, "srestore end", "srestore end ok"))
		return -1;
	return 0;
}

#ifndef _CHAMP_SERVER_H
#define _CHAMP_SERVER_H

#define FILENO_LEN	8

extern int champ_chooser_server(struct sdirs *sdirs, struct conf *conf);
extern int champ_chooser_server_standalone(const char *sclient,
	struct conf *conf);

#endif

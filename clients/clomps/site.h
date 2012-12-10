#ifndef SITE_INCLUDED_H
#define SITE_INCLUDED_H


#define TMP_TEMPLATE ".clomps.XXXXXX"

extern unsigned int num_sites;
extern unsigned int num_done;
extern unsigned int num_sitelist;


struct site_struct {
	char *name;
	int siteid;
	unsigned int sid;

	unsigned int num_dirs;
	char **dirs;
	unsigned int current_dir;

	time_t last_check;
	time_t last_check_autoq; // We clear last_check early, so save it for autoq
 	int failed;
    int skip;
	lion64_t size;

	unsigned int num_files;
	file_t **files;

	int use_lists;
	char *dskiplist;
	char *dpasslist;
	char *fskiplist;
	char *fpasslist;
	int fskipempty;

	char *inctest;
	char *nuketest;
};

typedef struct site_struct site_t;

extern site_t **sites;

enum status_enum {
	STATUS_MISS = 0,
	STATUS_INC,
	STATUS_COMPLETE,
	STATUS_FAIL,
    STATUS_NUKED,
	STATUS_SKIP
};

typedef enum status_enum status_t;

extern file_t **new_files;
extern unsigned int num_new;


void site_cmd_welcome(char **, char **, int, void *);
void site_cmd_ssl(char **, char **, int, void *);
void site_cmd_auth(char **, char **, int, void *);
void site_cmd_sitelist(char **, char **, int, void *);
void site_cmd_sessionnew(char **, char **, int, void *);
void site_cmd_disconnect(char **, char **, int, void *);
void site_cmd_connect(char **, char **, int, void *);
void site_cmd_cwd(char **, char **, int, void *);
void site_cmd_dirlist(char **, char **, int, void *);

int site_handler(lion_t *handle, void *user_data,
				 int status, int size, char *line);

void sites_compare(void);
status_t site_status(site_t *site, file_t *file);
int site_listsok(site_t *site, file_t *file);
void site_ready(lion_t *engine, site_t *site);
site_t *site_find(char *name);
char *status2str(status_t i);
void site_savecfg(void);

#endif

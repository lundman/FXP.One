#ifndef SITEMGR_H_INCLUDED
#define SITEMGR_H_INCLUDED


// SITE structure as defined by FXP.One 0.1
struct site_struct {
	char *name;
	char *host;
	int port;
	char *iface;
	int iport;
	char *user;
	char *pass;
	int passive;
	int fxp_passive;
	int control_TLS;
	int data_TLS;
	int desired_type;
	int resume;
	int resume_last;
	int pret;
	char *fskiplist;
	char *dskiplist;
	char *fpasslist;
	char *dpasslist;
	char *fmovefirst;
	char *dmovefirst;
	int fskipempty;
	int dskipempty;
	
	// Client defined own
	int items;
	char **keys;
	char **values;

	//Internal fields start here
	int siteid;
	struct site_struct *next;
};

typedef struct site_struct site_t;









void    sitemgr_draw          ( void );
void    sitemgr_undraw        ( void );
void    sitemgr_draw_4real    ( void );
site_t *sitemgr_getby_index   ( unsigned int );
void    sitemgr_poll          ( void );
void    sitemgr_set_left      ( site_t * );
void    sitemgr_set_right     ( site_t * );
int     sitemgr_checkclick    ( MEVENT * );

void    sitemgr_draw_edit     ( void );
void    sitemgr_undraw_edit   ( void );
void    sitemgr_save          ( void );

#endif

// $Id: local.h,v 1.2 2006/03/18 06:34:20 lundman Exp $
// 
// Jorgen Lundman 14th March 2006.

#ifndef LOCAL_H_INCLUDED
#define LOCAL_H_INCLUDED

#include "lion.h"



lion_t *local_spawn(void *);



#ifdef LOCAL_WITH_DECL
static int local_handler      ( lion_t *, void *, int, int, char *);
static int local_data_handler ( lion_t *, void *, int, int, char *);
static int local_file_handler ( lion_t *, void *, int, int, char *);
void       local_cmd_auth     ( lion_t *, char *, char * );
void       local_cmd_user     ( lion_t *, char *, char * );
void       local_cmd_pass     ( lion_t *, char *, char * );
void       local_cmd_syst     ( lion_t *, char *, char * );
void       local_cmd_clnt     ( lion_t *, char *, char * );
void       local_cmd_feat     ( lion_t *, char *, char * );
void       local_cmd_type     ( lion_t *, char *, char * );
void       local_cmd_pbsz     ( lion_t *, char *, char * );
void       local_cmd_prot     ( lion_t *, char *, char * );
void       local_cmd_ccsn     ( lion_t *, char *, char * );
void       local_cmd_pasv     ( lion_t *, char *, char * );
void       local_cmd_port     ( lion_t *, char *, char * );
void       local_cmd_cwd      ( lion_t *, char *, char * );
void       local_cmd_pwd      ( lion_t *, char *, char * );
void       local_cmd_list     ( lion_t *, char *, char * );
void       local_cmd_rest     ( lion_t *, char *, char * );
void       local_cmd_stor     ( lion_t *, char *, char * );
void       local_cmd_appe     ( lion_t *, char *, char * );
void       local_cmd_retr     ( lion_t *, char *, char * );
void       local_cmd_size     ( lion_t *, char *, char * );
void       local_cmd_mdtm     ( lion_t *, char *, char * );
void       local_cmd_noop     ( lion_t *, char *, char * );
void       local_cmd_mkd      ( lion_t *, char *, char * );
void       local_cmd_rmd      ( lion_t *, char *, char * );
void       local_cmd_quit     ( lion_t *, char *, char * );
void       local_cmd_rnfr     ( lion_t *, char *, char * );
void       local_cmd_rnto     ( lion_t *, char *, char * );
void       local_cmd_dele     ( lion_t *, char *, char * );
void       local_cmd_abor     ( lion_t *, char *, char * );
void       local_cmd_site     ( lion_t *, char *, char * );

#undef LOCAL_WITH_DECL
#endif



#endif

#ifndef CONF_INCLUDED_H
#define CONF_INCLUDED_H

extern char *conf_incpattern;
extern char *conf_autoqpattern;
extern char *conf_file_name;
extern time_t default_age;
extern int conf_do_autoq;
extern int conf_do_save;
extern int conf_do_verbose;





void options(char *prog);
void arguments(int argc, char **argv);
void conf_fxpone(char **keys, char **values, int items,void *optarg);
void conf_site(char **keys, char **values, int items,void *optarg);
void conf_timestamp(char **keys, char **values, int items,void *optarg);
void conf_autoq(char **keys, char **values, int items,void *optarg);
void conf_continued(char **keys, char **values, int items,void *optarg);
void conf_irc(char **keys, char **values, int items,void *optarg);
void conf_trade(char **keys, char **values, int items,void *optarg);
void conf_read(void);
void conf_time(char *timefile);
void conf_save(lion_t *savefile);




#endif

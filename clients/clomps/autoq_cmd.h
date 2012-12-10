#ifndef AUTOQ_CMD_INCLUDED_H
#define AUTOQ_CMD_INCLUDED_H





int autoq_cmd_handler(lion_t *handle, void *user_data,
					  int status, int size, char *line);
void autoq_cmd_sessionnew(char **keys, char **values, int items,void *optarg);
void autoq_cmd_disconnect(char **keys, char **values, int items,void *optarg);
void autoq_cmd_connect(char **keys, char **values, int items,void *optarg);
void autoq_cmd_queuenew(char **keys, char **values, int items,void *optarg);
void autoq_cmd_go(char **keys, char **values, int items,void *optarg);
void autoq_cmd_qc(char **keys, char **values, int items,void *optarg);
void autoq_cmd_qs(char **keys, char **values, int items,void *optarg);

void autoq_send_qadd  ( autoq_t *aq );


#endif

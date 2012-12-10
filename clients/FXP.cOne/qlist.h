#ifndef QLIST_H_INCLUDED
#define QLIST_H_INCLUDED



struct qlist_struct {
	unsigned int qid;
	char *left_name;
	char *right_name;
	struct qlist_struct *next;
};

typedef struct qlist_struct qlist_t;






int          qlist_checkclick        ( MEVENT * );
void         qlist_draw              ( void );
void         qlist_undraw            ( void );
void         qlist_poll              ( void );
CDKSCROLL   *qlist_get_viewer        ( void );

#endif

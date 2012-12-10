// $Id: queue.h,v 1.17 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 18th March 2004.

#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#ifndef WIN32
#include <sys/time.h>
#else
#define _WINDOWS_MEAN_AND_LEAN
#include <winsock2.h>
#endif



#include "file.h"
#include "manager.h"

// Defines.


//
// Defining this means each queue process step will be at least
// one second apart. Great for debugging.
//
//#define SLOW_QUEUE








enum queue_state_enum {
	QUEUE_IDLE = 0,   // idle, controlled by user.
	QUEUE_START,
	QUEUE_ISCONNECTED,
	QUEUE_START_NEXT_ITEM,
	QUEUE_ACTIVE,
	QUEUE_FILE_START,

	QUEUE_FILE_PHASE_1_START,
	QUEUE_FILE_PHASE_1_IDLE,

	QUEUE_FILE_PHASE_2_START,
	QUEUE_FILE_PHASE_2_WAIT,
	QUEUE_FILE_PHASE_2_DISABLE,  // 10

	QUEUE_FILE_PHASE_3_START,
	QUEUE_FILE_PHASE_3_WAIT,

	QUEUE_FILE_PHASE_4_START,

	QUEUE_FILE_PHASE_5_START,
	QUEUE_FILE_PHASE_5_REST_WAIT,
	QUEUE_FILE_PHASE_5_REST,

	QUEUE_FILE_PHASE_6_START,

	QUEUE_FILE_PHASE_7_START,
	QUEUE_FILE_PHASE_7_WAIT,

	QUEUE_FILE_PHASE_8_START,   // 20
	QUEUE_FILE_PHASE_8_WAIT,

	QUEUE_FILE_PHASE_9_START,
	QUEUE_FILE_PHASE_9_REST,

	QUEUE_FILE_PHASE_10_START,
	QUEUE_FILE_PHASE_10_WAIT,

	QUEUE_FILE_PHASE_11_START,
	QUEUE_FILE_PHASE_11_WAIT,

	QUEUE_FILE_PHASE_12_START,




	QUEUE_DIRECTORY_START,

	QUEUE_DIRECTORY_PHASE_1_IDLE,  // 30

	QUEUE_DIRECTORY_PHASE_2_START,

	QUEUE_DIRECTORY_PHASE_2_MKD,

	QUEUE_DIRECTORY_PHASE_2_COMPARE,


    // Was started but can't start due to MAX_QUEUES and other queues
    // are already busy. When another queue enters QUEUE_EMPTY, we
    // will release a queue in OTHER_IS_BUSY state.
	QUEUE_OTHER_IS_BUSY,



	QUEUE_ITEM_FAILED,
	QUEUE_REQUEUE_ITEM,

	QUEUE_ERROR_ITEM,

	QUEUE_REMOVE_ITEM,
	QUEUE_EMPTY
};

typedef enum queue_state_enum queue_state_t;




// Variables



enum qitem_type_enum {
	QITEM_TYPE_NONE = 0,
	QITEM_TYPE_FILE,
	QITEM_TYPE_DIRECTORY,
	QITEM_TYPE_STOP
};

typedef enum qitem_type_enum qitem_type_t;


#define QITEM_FLAG_RESUME    1  // The default is to RESUME.
#define QITEM_FLAG_OVERWRITE 2  // The default is to RESUME.
#define QITEM_FLAG_REQUEUED  4  // Resume last re-queued


struct qitem_struct {

	qitem_type_t type;

	file_t src;
	file_t dst;

	int src_is_north;

    int weight;

	unsigned int flags;

	// Failure information.
	unsigned int src_failure;
	unsigned int dst_failure;

	unsigned int soft_errors;

	char **src_reasons;
	char **dst_reasons;

	unsigned int src_num_reasons;
	unsigned int dst_num_reasons;

	//unsigned int src_dirlist;
	//unsigned int dst_dirlist;
	unsigned int src_transfer;  // 1 started, 2 finished
	unsigned int dst_transfer;  // 1 started, 2 finished

	struct qitem_struct *next;

};

typedef struct qitem_struct qitem_t;




struct queue_struct {

	unsigned int id;     // QID of this queue.

	queue_state_t state;
	unsigned int stop;

	unsigned int north_sid;
	unsigned int south_sid;

	manager_t *north_mgr;
	manager_t *south_mgr;

	manager_t *src_mgr;       // These are only set during transfer time.
	manager_t *dst_mgr;

	unsigned int num_items;
	qitem_t *items;           // linkedlist of the items in the queue.

	unsigned int num_errors;
	qitem_t *err_items;       // linkedlist of failed items.

	unsigned int secure_data;
	unsigned int srcpasv;     // For current item, is src to get PASV, or dst.
	char *pasv_line;          // Remember the PASV reply for PORT.


	// Subscribed users for events. These needs to be cleared if user drops!
	unsigned int num_users;
	command2_node_t **users;

	unsigned int mkd_depth;   // depth of MKD's required for first file.

	struct timeval xfr_start;
	struct timeval xfr_end;

	float last_cps;

	struct queue_struct *next;
};

typedef struct queue_struct queue_t;



// Functions

int                  queue_init                ( void );
void                 queue_free                ( void );

queue_t             *queue_newnode             ( void );
void                 queue_freenode            ( queue_t * );
void                 queue_unlink              ( queue_t * );

qitem_t             *queue_newitem             ( queue_t * );
void                 queue_unchain             ( queue_t *, qitem_t * );
void                 queue_freeitem            ( qitem_t * );

qitem_type_t         queue_str2type            ( char * );
char                *queue_type2str            ( qitem_type_t );
queue_t             *queue_findbyqid           ( unsigned int );

int                  queue_insert              ( queue_t *, qitem_t *,
												 char *, lion_t * );
void                 queue_list                ( command2_node_t * );
void                 queue_get                 ( queue_t *, lion_t * );
void                 queue_err                 ( queue_t *, lion_t * );

void                 queue_grab                ( queue_t *, command2_node_t *, int );
void                 queue_del                 ( queue_t *, char *, lion_t *);
void                 queue_move                ( queue_t *, char *, char *,lion_t *);
void                 queue_compare             ( queue_t *, lion_t *);

void                 queue_go                  ( queue_t *, lion_t * );
void                 queue_stop                ( queue_t *, lion_t * , int );

void                 queue_run                 ( void );

void                 queue_adderr_src          ( qitem_t *, char *);
void                 queue_adderr_dst          ( qitem_t *, char *);
void                 queue_directory_think     ( queue_t *,
												 manager_t *, manager_t *,
												 char *, lion_t * );
int                  queue_skiplist            ( sites_t *, file_t * );

void                 queue_subscribe           ( queue_t *, command2_node_t *);
int                  queue_is_subscriber       ( queue_t *, command2_node_t *);
void                 queue_unsubscribe         ( queue_t *, command2_node_t *);
void                 queue_unsubscribe_all     ( command2_node_t *);
void                 queue_calc_speed          ( queue_t *, float *, float * );
int                  queue_isidle              ( queue_t * );
void                 queue_xdupe_item          ( queue_t *, char * );
queue_t             *queue_clone               ( queue_t *, command2_node_t * );
char *get_mkdname_by_depth(queue_t *queue, int decrease);


#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lion.h"
#include "misc.h"

#include "file.h"
#include "site.h"
#include "clomps.h"
#include "autoq.h"
#include "autoq_cmd.h"
#include "parser.h"
#include "debug.h"



static parser_command_list_t autoq_command_list[] = {
        { COMMAND( "SESSIONNEW" ), autoq_cmd_sessionnew  },
		{ COMMAND( "DISCONNECT" ), autoq_cmd_disconnect  },
		{ COMMAND( "CONNECT" ),    autoq_cmd_connect     },
		{ COMMAND( "QUEUENEW" ),   autoq_cmd_queuenew    },
		{ COMMAND( "GO" ),         autoq_cmd_go          },
		{ COMMAND( "QC" ),         autoq_cmd_qc          },
		{ COMMAND( "QS" ),         autoq_cmd_qs          },
        { NULL,  0         ,                    NULL }
};



int autoq_cmd_handler(lion_t *handle, void *user_data,
					  int status, int size, char *line)
{
	fxpone_t *fxpone = (fxpone_t *)user_data;

	switch(status) {

	case LION_CONNECTION_LOST:
		printf("Failed to connect to FXP.One\n");
		do_exit=1;
		if (fxpone)
			fxpone->handle = NULL;
		break;

	case LION_CONNECTION_CLOSED:
		printf("Connection closed to FXP.One\n");
		do_exit=1;
		if (fxpone)
			fxpone->handle = NULL;
		break;

	case LION_INPUT:
		debugf("FXP.One: %s\n", line);
		parser_command(autoq_command_list, line, (void *)user_data);
	}

	return 0;

}






// << SESSIONNEW|SITEID=0|SID=1
void autoq_cmd_sessionnew(char **keys, char **values, int items,void *optarg)
{
	char *siteid, *sid;

	sid    = parser_findkey(keys, values, items, "SID");
	siteid = parser_findkey(keys, values, items, "SITEID");

	if (!siteid) return;

	autoq_assign_sid(atoi(siteid), sid);

}


// << DISCONNECT|SID=1|MSG=Undefined error: 0
void autoq_cmd_disconnect(char **keys, char **values, int items,void *optarg)
{
	char *sid;
	autoq_t *aq;

	sid    = parser_findkey(keys, values, items, "SID");
	if (!sid) return;

	aq = autoq_find_by_sid(atoi(sid));
	if (!aq) return;

	aq->status = AUTOQ_FAILED;

	debugf("Disconnect received for %s\n", sid);

}


// << CONNECT|SID=1
void autoq_cmd_connect(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *fxpone = (fxpone_t *)optarg;
	char *sid;
	autoq_t *aq;

	sid    = parser_findkey(keys, values, items, "SID");
	if (!sid) return;

	aq = autoq_find_by_sid(atoi(sid));
	if (!aq) return;


	if (aq->status == AUTOQ_CONNECTED_NONE) {
		aq->status = AUTOQ_CONNECTED_ONE;
		debugf(" %s->%s: Connected one...\n",
			   aq->from, aq->to);
		return;
	}

	aq->status = AUTOQ_CONNECTED_BOTH;


	debugf(" %s->%s: Connected two, creating queue...\n",
		   aq->from, aq->to);

	// Create a queue!
	lion_printf(fxpone->handle,
				"QUEUENEW|NORTH_SID=%u|SOUTH_SID=%u\r\n",
				aq->src_site->sid,
				aq->dst_site->sid);

}



//<< QUEUENEW|CODE=0|QID=1|MSG=Queue created.
void autoq_cmd_queuenew(char **keys, char **values, int items,void *optarg)
{
	//fxpone_t *fxpone = (fxpone_t *)optarg;
	char *qid, *sid;
	autoq_t *aq;

	sid    = parser_findkey(keys, values, items, "NORTH_SID");
	if (!sid) return;

	aq = autoq_find_by_sid(atoi(sid));
	if (!aq) return;

	qid    = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		printf(" %s -> %s queue creation failed.\n",
			   aq->from, aq->to);
		aq->status = AUTOQ_FAILED;
		return;
	}

	aq->qid = atoi(qid);

    autoq_send_qadd(aq);

}



void autoq_send_qadd(autoq_t *aq)
{
	int i, current_dir;
	file_t *file;

    // Only send if we are ready.
	if (aq->status != AUTOQ_CONNECTED_BOTH)
        return;

    if (aq->qid <= 0)
        return;

	// Load queue!
	debugf(" %s->%s: loading queue\n", aq->from, aq->to);

	// For all items in the autoq list;
	/// attempt to find src node for current_dir value
	//// send QADD for item, insert at front (reverse order).

	for (i = 0; i < aq->num_files; i++) {

        // Only send new entries
        if (aq->files[i]->queued) continue;
        aq->files[i]->queued = 1;

		// attempt to locate file for right source path.
		file = file_find(aq->src_site->files,
						 aq->src_site->num_files,
						 aq->files[i]->name);
		//current_dir = file ? file->current_dir : 0;
		current_dir = file ? file->current_dir : aq->dst_site->current_dir;


		if (!lion_printf(aq->fxpone->handle,
                         "QADD|QID=%u|@=LAST|SRC=NORTH|"
                         "QTYPE=%s|"
                         "SRCPATH=%s/%s|"
                         "DSTPATH=%s/%s|"
                         "SRCSIZE=%"PRIu64
                         "\r\n",
                         aq->qid,
                         aq->files[i]->type ? "DIRECTORY" : "FILE",
                         aq->src_site->dirs[current_dir],
                         aq->files[i]->name,
                         aq->dst_site->dirs[aq->dst_site->current_dir],
                         aq->files[i]->name,
                         aq->files[i]->size)) return;


	}

	debugf(" %s->%s: done.\n", aq->from, aq->to);

	// Send GO.
#if 1
    if (!aq->running) {
        lion_printf(aq->fxpone->handle,
                    "GO|SUBSCRIBE|QID=%u\r\n",
                    aq->qid);
    }
#else

	aq->status = AUTOQ_COMPLETE;

#endif

}


//<< GO|QID=1|OK|MSG=Queue processing started.
void autoq_cmd_go(char **keys, char **values, int items,void *optarg)
{
	autoq_t *aq;
	char *qid;

	qid   = parser_findkey(keys, values, items, "QID");
	if (!qid) return;

	aq = autoq_find_by_qid(atoi(qid));
	if (!aq) return;

    aq->running = 1;

	printf(" %s->%s: processing started.\n",
		   aq->from, aq->to);

}


//<< GO|QID=1|OK|MSG=Queue processing started.
void autoq_cmd_qc(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *fxpone = (fxpone_t *)optarg;
	autoq_t *aq;
	char *qid, *empty, *errors;
    int i;

	qid   = parser_findkey(keys, values, items, "QID");
	if (!qid) return;

	aq = autoq_find_by_qid(atoi(qid));
	if (!aq) return;

	empty   = parser_findkey(keys, values, items, "EMPTY");
	errors  = parser_findkey(keys, values, items, "ERRORS");

	if (empty) {
		printf(" %s->%s: processing completed. (REQUEUE=%s, Errors=%s)\n",
			   aq->from, aq->to, aq->requeue ? "on" : "off",
               errors?errors:"none");

        aq->running = 0;

        // Should we requeue things? Are there any to requeue?
        if (aq->requeue) {
            int requeue = 0;

            for (i = 0; i < aq->num_files; i++) {
                if (aq->files[ i ]->transfers) {
                    aq->files[ i ]->queued = 0;    // make it be queued again
                    aq->files[ i ]->transfers = 0; // reset counter
                    requeue = 1;
                    printf("Requeueing '%s' due to transfers\n",
                           aq->files[ i ]->name);
                }
            }
            if (requeue) {
                autoq_start(aq);
                return;
            }
        }

        // If we have added items here, that we've not processed, do so now
        for (i = 0; i < aq->num_files; i++) {
            if (!aq->files[ i ]->queued) {
                printf("Re-starting queue for new items\n");
                autoq_start(aq);
                return;
            }
        }

		aq->status = AUTOQ_COMPLETE;
		lion_printf(fxpone->handle,
					"QUEUEFREE|QID=%u\r\n",
					aq->qid);
        aq->qid = 0;
		// Also, run through the files in aq, and add to dst's file list.
		// that way they will appear as new incase dst is used as src
		// in later passnums.

		// This entry should be queued.
        // 20100427: There is a bug here that triggers sometime,
        // most likely because we realloc space + 1, rather than + num_files.
#if 0 // this code can core dump 2012
		tmp = realloc(aq->dst_site->files,
					  sizeof(file_t *)*(aq->dst_site->num_files + aq->num_files));
        //sizeof(file_t *)*(aq->dst_site->num_files + 1));
		if (!tmp) return;

		aq->dst_site->files = tmp;
		memcpy(&aq->dst_site->files[ aq->dst_site->num_files ],
			   aq->files,
			   aq->num_files * sizeof(file_t *));
		aq->dst_site->num_files+= aq->num_files;
#endif

	}

}



//<< QS|QID=1|START|@=0|SRCNAME=might_be_game_sounds.pcm
//<< QS|QID=1|XFREND
//
// Since the START message has the path, we safe that for later.
// The IF we get a XFREND, we know it was a file, and increase transfers.
void autoq_cmd_qs(char **keys, char **values, int items,void *optarg)
{
	autoq_t *aq;
	char *qid, *start, *srcname, *KBs, *timex;
    int i, d, q, len, len2;

	qid   = parser_findkey(keys, values, items, "QID");
	if (!qid) return;

	aq = autoq_find_by_qid(atoi(qid));
	if (!aq) return;

	start   = parser_findkey(keys, values, items, "START");
	srcname = parser_findkey(keys, values, items, "SRCPATH");

	if (start && srcname && *srcname) {
        SAFE_FREE(aq->start_path);
        SAFE_COPY(aq->start_path, srcname);
        return;
    }


	start   = parser_findkey(keys, values, items, "XFREND");
	KBs     = parser_findkey(keys, values, items, "KB/s");
	timex    = parser_findkey(keys, values, items, "TIME");
	srcname = aq->start_path;

	if (start && srcname && *srcname) {

        // If we got a START message for a file, attempt to find the queue
        // item for this transfer in the aq file list. If found, increase
        // the "transfers" counter. This is to aid with an attempt to requeue
        // until completed. If transfers=0, no need to requeue.
        // This isn't very accurate, and can re-add items when not needed,
        // but this isn't considered damaging anyway. It will simply do nothing
        // if everything is already transferred.
        debugf("CHECKING transfer complete on '%s' (%s KB/s)\n", srcname,
               KBs? KBs : "");

        // $DIR/$QUEUEITEM/sub/filename
        // so skip past $DIR, then any "/"s.
        // then take the entry name

        // Find SITEs where DIR match
        for (i = 0; i < num_sites; i++) {
            if (!sites[i]) continue;

            //debugf("XX: %s\n", sites[i]->name);

            // For all defined dirs
            for (d = 0; d < sites[i]->num_dirs; d++) {
                len = strlen(sites[i]->dirs[d]);

                //debugf("  : dir %s\n", sites[i]->dirs[d]);

                if (!strncmp(sites[i]->dirs[d], srcname, len)) {
                    // dirs part matches,
                    while(srcname[len] == '/') len++; // Skip past all slashes

                    // Do we have at least 1 character name?
                    if (strlen(&srcname[len]) <= 0) continue;

                    // Loop all file items in this autoq, if match, increase
                    // counter.

                    for (q = 0; q < aq->num_files; q++) {

                        //debugf("  : entry %s\n", aq->files[ q ]->name);

                        len2 = strlen(aq->files[ q ]->name);

                        if (!strncmp(aq->files[ q ]->name, &srcname[len],
                                     len2)) {

                            // Match
                            while(srcname[len + len2] == '/') len2++;

                            // We only want to see items inside the queue dir
                            // since the dir also get a START
                            if (strlen(&srcname[len + len2]) <= 0) continue;

                            // Occasionally report speeds
							time_t now;
							time(&now);
                            if ((!aq->last_info ||
									(now - aq->last_info > 120)) &&
                                timex &&
                                KBs) {
								if (atoi(KBs) >= 10) {
									printf("Queue processing..: %ss %sKB/s\n",
										timex, KBs);
									aq->last_info = now;
								}
							}
                            aq->files[ q ]->transfers++;
                            debugf("[autoq] increased transfers on '%s' to %d\n",
                                   aq->files[ q ]->name,
                                   aq->files[ q ]->transfers);

                        } // If filename match

                    } // for all "q" in aq->files

                } // If DIR names match

            } // for all "d" in num_dirs of site

        } // for all "i" in sites

    } // if start and srcname

}

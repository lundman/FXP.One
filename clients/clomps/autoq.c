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
#include "debug.h"
#include "conf.h"

static autoq_t *autoq_head = NULL;
static int max_pass = 0;

static int current_pass = 0;  // incremented first, start with 1.


autoq_t *autoq_find_by_sid(unsigned int sid)
{
	autoq_t *result;

	for (result = autoq_head; result; result = result->next) {

		if (result->src_site && result->src_site->sid == sid)
			return result;
		if (result->dst_site && result->dst_site->sid == sid)
			return result;

	}

	return NULL;

}


autoq_t *autoq_find_by_qid(unsigned int qid)
{
	autoq_t *result;

	for (result = autoq_head; result; result = result->next) {

		if (result->qid == qid)
			return result;

	}

	return NULL;

}


//
// The idea is that we have received siteid->sid mapping. Attempt to
// find first best fit. Since order isn't so important, we pick first
// applicable, that has a matching pass number.
//
void autoq_assign_sid(unsigned int siteid, char *sid)
{
	autoq_t *result;

	//debugf("assigning siteid %u\n", siteid);

	for (result = autoq_head; result; result = result->next) {

		if (result->passnum != current_pass) continue;

		// It is either source, or dst
		if ((result->src_site->siteid == siteid) &&
			(result->src_site->sid == 0)) {

			if (!sid) {
				printf("Site failed: %s\n", result->from);
				result->src_site->failed = 1;
				return;
			}

			result->src_site->sid = atoi(sid);
			return;

		} else if ((result->dst_site->siteid == siteid) &&
				   (result->dst_site->sid == 0)) {

			if (!sid) {
				printf("Site failed: %s\n", result->to);
				result->dst_site->failed = 1;
				return;
			}

			result->dst_site->sid = atoi(sid);
			return;
		}

	} // all q

}





autoq_t *autoq_newnode(void)
{
	autoq_t *result;

	result = malloc(sizeof(*result));

	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	result->next = autoq_head;
	autoq_head = result;

	return result;

}


void autoq_freenode(autoq_t *aq)
{
    autoq_t *node, *prev;
    int i;

    // First, remove from linkedlist.
    for (node = autoq_head, prev = NULL;
         node;
         prev = node, node = node->next) {

        if (node == aq) {
            if (!prev)
                autoq_head = node->next;
            else
                prev->next = node->next;
        }
    }


    // Free all resources
    for (i = 0; i < aq->num_files; i++) {
        file_free( aq->files[ i ] );
        aq->files[ i ] = NULL;
    }

    aq->num_files = 0;
    SAFE_FREE(aq->from);
    SAFE_FREE(aq->to);
    SAFE_FREE(aq->accept);
    SAFE_FREE(aq->reject);
    SAFE_FREE(aq->start_path);
    aq->fxpone   = NULL;
    aq->src_site = NULL;
    aq->dst_site = NULL;

    // Free node
    SAFE_FREE(aq);
}

/*
 * Add fields to node. Allow function to be called repeatedly with
 * NULL passnum/from/to and use last created node to append values to
 * accept, reject and hide.
 */

void autoq_add(char *passnum, char *from, char *to, char *accept, char *reject,
               char *incskip, char *requeue)
{
	static autoq_t *node = NULL;
    char *tmp;

    // Create new node if setting up a new autoq
    if (passnum && from && to) {
        node = autoq_newnode();

        if (!node) return;

        node->passnum = atoi(passnum);
        SAFE_COPY(node->from,   from);
        SAFE_COPY(node->to,     to);
        if (incskip && *incskip)
            node->incskip = 1;
        if (requeue && *requeue)
            node->requeue = 1;

        if (node->passnum > max_pass)
            max_pass = node->passnum;
    }


    if (!node) return;

    // Add in new values for last node
    if (accept) {
        if (!node->accept) {
            SAFE_COPY(node->accept, accept);
        } else {
            tmp = node->accept;
            node->accept = misc_strjoin(tmp?tmp:"", accept);
            SAFE_FREE(tmp);
        }
    }

    if (reject) {
        if (!node->reject) {
            SAFE_COPY(node->reject, reject);
        } else {
            tmp = node->reject;
            node->reject = misc_strjoin(tmp?tmp:"", reject);
            SAFE_FREE(tmp);
        }
    }

}








//
// If we have no QID we need to create one
// If we have no SID, we need to connect them
// Send QADD
// Send GO
//
void autoq_start(autoq_t *aq)
{

	printf(" %s->%s: queued %d item%s...\n",
		   aq->from,
		   aq->to,
		   aq->num_files,
		   aq->num_files == 1 ? "" : "s" );



	// Change handler over to us.
	lion_set_handler(aq->fxpone->handle, autoq_cmd_handler);


    if (aq->qid) { // Have QID
        // Just QADD, and GO
        printf(" %s->%s: queue active, adding to queue...\n",
               aq->from,
               aq->to);
        autoq_send_qadd(aq);
        return;
    }

	// Ask fxpone to connect both sites.
	// create queue
	// issue go
	printf(" %s->%s: connecting...\n",
		   aq->from,
		   aq->to);

	aq->status = AUTOQ_CONNECTED_NONE;
	aq->src_site->sid = 0;
	aq->dst_site->sid = 0;

	lion_printf(aq->fxpone->handle,
				"SESSIONNEW|SITEID=%u\r\n",
				aq->src_site->siteid);
	lion_printf(aq->fxpone->handle,
				"SESSIONNEW|SITEID=%u\r\n",
				aq->dst_site->siteid);


}








void autoq_process(fxpone_t *fxpone)
{

	int j;
	status_t st;
	autoq_t *aq;
	site_t *src, *dst;
	file_t **tmp;

	if (current_pass == 1)
		printf("AutoQueue armed. Max passes = %d.\n", max_pass);


	printf("\n********\nAutoQueue.. processing pass number %u\n", current_pass);

	// For each autoq in this pass;
	/// for all entries in src, that is missing, or incomplete, in dst;
	//// that also match accept, and don't match reject
	///// tag, connect, queue and go.
	for ( aq = autoq_head; aq; aq=aq->next) {

		if (aq->passnum != current_pass) continue;

		src = site_find(aq->from);
		dst = site_find(aq->to);

		// Couldn't find either site, skip
		if (!src || !dst) {
			printf("Error: AUTOQ specified for non-existant site.\n");
			printf("       This usually means you are missing a SITE line for the site in question.\n");
			aq->status = AUTOQ_COMPLETE;
			continue;
		}

		// Either site failed, skip
		if (src->failed || dst->failed) {
			printf(" %s->%s: One or both sites has failed, skipping...\n",
				   aq->from,
				   aq->to);
			aq->status = AUTOQ_COMPLETE;
			continue;
		}

        // In clomps autoq, DST is always to DIR[0]
        dst->current_dir = 0;


		// For all entries in new_files
		// check they exist in src
		aq->num_files = 0;

		for (j = 0; j < num_new; j++) {


			st = site_status(src, new_files[j]);

			// Do we queue to move it if it is incomplete? Probably
			if ((st != STATUS_COMPLETE) &&
				(st != STATUS_INC)) continue;

            // If not asked to move incompletes...
            if ((aq->incskip == 1) && (st == STATUS_INC)) continue;

            // If asked to queue it from cmdline (-I pattern) we add
            // it regardless.
            if (!conf_autoqpattern
                || !file_listmatch(conf_autoqpattern, new_files[j]->name)) {

                // Since items are in the list if they are new ANYWHERE, we
                // need to filter out only items that are new on SOURCE. Or
                // we end up resending things all over.
                if (new_files[j]->date <= src->last_check_autoq) continue;

                st = site_status(dst, new_files[j]);

                // If it was nuked on dst, ignore it
                if (st == STATUS_NUKED) continue;

                // It could be MISS or INC on dst
                if ((st != STATUS_MISS) &&
                    (st != STATUS_INC)) continue;

                // Check that it matches Accept.
                if (!file_listmatch(aq->accept, new_files[j]->name))
                    continue;

                // Check is passes the Reject list.
                if (aq->reject && file_listmatch(aq->reject,
                                                 new_files[j]->name))
				continue;

            } // cmdline autoq pattern

			// This entry should be queued.
			tmp = realloc(aq->files,
						  sizeof(file_t *)*(aq->num_files + 1));
			if (!tmp) continue;

			aq->files = tmp;
			aq->files[ aq->num_files ] = file_dupe(new_files[j]);
			aq->num_files++;

			if (conf_do_verbose)
				printf(" %s->%s: queued: %s\n",
					   aq->from,
					   aq->to,
					   new_files[j]->name);


		} // for all new_files

		aq->fxpone   = fxpone;
		aq->src_site = src;
		aq->dst_site = dst;

		if (aq->num_files)
			autoq_start(aq);
		else
			aq->status = AUTOQ_COMPLETE;

	} // for all autoq, in this pass

	// If we have another pass to do, we wait here for the pass to finish.


}


//
// Check if all the queues in this passnum has finished processing.
// If so, bump the passnum to process the next load.
//
void autoq_check(fxpone_t *fxpone)
{
	autoq_t *aq;

	// Detect if any queue is active.
	for ( aq = autoq_head; aq; aq=aq->next) {

		if (aq->passnum != current_pass) continue;

		if (aq->status >= AUTOQ_COMPLETE) continue;

		return;
	}

	//
	current_pass++;

	autoq_process(fxpone);

	if (current_pass > max_pass)
		do_exit = 2;


}

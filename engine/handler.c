#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#ifndef WIN32
#include <inttypes.h>
#endif

// Alas, we use "PRIu64" to work out how to print 64bit values, but the
// FreeBSD, pre 5 release, does not define these.
#ifdef __FreeBSD__
#ifndef PRIu64
#define PRIu64 "qu"
#endif
#endif
#ifdef WIN32
#ifndef PRIu64
#define PRIu64 "I64d"
#endif
#endif

#include "lion.h"
#include "misc.h"

#include "debug.h"
#include "engine.h"
#include "session.h"
#include "file.h"
#include "manager.h"
#include "handler.h"
#include "queue.h"




//
// Translate FTP codes to FXP.One codes. Ie, if its
// -1 (command continuation) send -1.
// if code is code (==okcode) or < 300, then send 0
// for failures, send same code back
int handler_codeok(int reply, int okcode)
{
	if (reply < 0) return reply;
	if (okcode && (reply == okcode)) return 0;
	if (!okcode && (reply < 300)) return 0;
	return reply;
}









//
// Main session handler when we have a user as the owner.
//
void handler_withuser(session_t *session, int event, int id, int reply,
					  char *line)
{
	lion_t *huser = NULL;
	manager_t *node = NULL;
	int i;

	// If this session doesnt have a manager, do we care?
	if (!(node = manager_find_fromsession(session)))
		return;

	if (node->user && node->user->handle)
		huser = node->user->handle;


	switch(event) {

	case SESSION_EVENT_LOST:
		debugf("[handler_withuser] lost event\n");

		// If the user owns it, notify them.
		if (huser && (node->locked == YNA_NO))
			lion_printf(huser, "DISCONNECT|SID=%u|MSG=%s\r\n",
						node->id, line ? line : "(null)");

		// We no longer have a session.
		node->old_session = node->session;
		node->session = NULL;
		break;

	case SESSION_EVENT_IDLE:
		debugf("[handler_withuser] idle event\n");
		// If its this sessions first, lets send a connected msg too




		// If the user owns it, notify them.
		if (huser && (node->locked == YNA_NO)) {

			if (!node->connected)
				lion_printf(huser, "CONNECT|SID=%u%s\r\n",
							node->id,
							(session_feat(node->session)&FEATURE_SSL) ?
							"|SSL" : "");

			node->connected = 1;

			lion_printf(huser, "IDLE|SID=%u\r\n",
						node->id);
		}
		break;


	case SESSION_EVENT_CMD:
		debugf("[handler_withuser]: id - %d %d\n", id, reply);


		switch(id) {

		case HANDLER_DIRLIST_SENDTO_USER:

			if (huser) {
				file_t **files = (file_t **) line;
				char *name;

				lion_printf(huser, "DIRLIST|SID=%u|BEGIN|items=%d\r\n",
							node->id, reply);
				// If we have RAW field, send it.
				for (i = 0; i < reply; i++) {

					name = misc_url_encode(files[i]->name);

					lion_printf(huser,
								"DIRLIST|SID=%u|FID=%u|NAME=%s|DATE=%lu|SIZE=%"PRIu64"|USER=%s|GROUP=%s|PERM=%s|FTYPE=%s%s%s\r\n",
								node->id,
								files[i]->fid,
								name,
								(unsigned long)files[i]->date,
								files[i]->size,
								files[i]->user,
								files[i]->group,
								files[i]->perm,
								files[i]->directory == YNA_YES ? "directory" :
								files[i]->soft_link == YNA_YES ? "link" : "file",
								files[i]->raw ? "|RAW=" : "",
								files[i]->raw ? files[i]->raw : "");

					SAFE_FREE(name);

				}
				lion_printf(huser, "DIRLIST|SID=%u|END\r\n", node->id);

			// It's free'd after this.
			}
			break; // dirlist

		case HANDLER_QUOTE_REPLY:
			if (huser)
				lion_printf(huser, "QUOTE|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 0),
							line ? line : "");
			break;

		case HANDLER_CWD_REPLY:
			if (huser)
				lion_printf(huser, "CWD|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 250),
							line ? line : "");
			break;

		case HANDLER_PWD_REPLY:
			if (huser) {
				char *path;

				path = session_pwd_reply(node->session,
										 reply,
										 line);
				if (path) path = misc_url_encode(path);

				lion_printf(huser, "PWD|SID=%u|CODE=%d|PATH=%s|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 257),
							path ? path : "",
							line);
			}
			break;

		case HANDLER_SIZE_REPLY:
			if (huser) {
				lion64u_t size;

				if (line && sscanf((char *)line, "213 %"PRIu64, &size))
					lion_printf(huser, "SIZE|SID=%u|CODE=%d|SIZE=%"PRIu64"|MSG=%s\r\n",
								node->id,
								handler_codeok(reply, 213),
								size, line ? line : "");
				else
					lion_printf(huser, "SIZE|SID=%u|CODE=%d|MSG=%s\r\n",
								node->id, reply, line ? line : "");
			}
			break;


		case HANDLER_DELE_REPLY:
			if (huser)
				lion_printf(huser, "DELE|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 257),
							line);

			break;

		case HANDLER_MKD_REPLY:
			if (huser)
				lion_printf(huser, "MKD|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 257),
							line);

			break;

		case HANDLER_RMD_REPLY:
			if (huser)
				lion_printf(huser, "RMD|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 257),
							line);

			break;

		case HANDLER_SITE_REPLY:
			if (huser)
				lion_printf(huser, "SITE|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 0),
							line);

			break;

		case HANDLER_RNFR_REPLY:
			if (reply == 350)
				node->user_flags |= MANAGER_USERFLAG_RNFR_OK;
			else if (huser)
				lion_printf(huser, "REN|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 350),
							line);

			break;

		case HANDLER_RNTO_REPLY:
			if (huser && (node->user_flags & MANAGER_USERFLAG_RNFR_OK))
				lion_printf(huser, "REN|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 250),
							line);

			break;

		case HANDLER_MDTM_REPLY:
			if (huser)
				lion_printf(huser, "MDTM|SID=%u|CODE=%d|MSG=%s\r\n",
							node->id,
							handler_codeok(reply, 213),
							line);

			break;



		default:
			printf("[handle_withuser] SESSION_EVENT_CMD called for id %d/%02X\n",
				   id, id);
			break;



		} // switch id

		break;


	case SESSION_EVENT_LOG:
		debugf("[handler_withuser] log: '%s'\n", line);

		if (huser && (node->user_flags&MANAGER_USERFLAG_LOG)) {
			char *encoded = NULL;

			if (line)
				encoded = misc_url_encode(line);

			lion_printf(huser, "LOG|SID=%u|MSG=%s\r\n",
						node->id, encoded ? encoded : "(null)");

			SAFE_FREE(encoded);
		}

		break;

	default:
		debugf("[handler_withuser] event %d\n", event);
		break;

	}

}




















//
// Main session handler when we have a user as the owner.
//
void handler_queue(session_t *session, int event, int id, int reply,
				   char *line)
{
	lion_t *huser = NULL;
	manager_t *node = NULL;
	queue_t *queue;
	int i;

	// If this session doesnt have a manager, do we care?
	if (!(node = manager_find_fromsession(session)))
		return;

	if (node->user && node->user->handle)
		huser = node->user->handle;

	// Find queue
	queue = queue_findbyqid(node->qid);
	if (!queue) return;


	switch(event) {

	case SESSION_EVENT_LOST:
		debugf("[handler_queue] lost event\n");

		// We no longer have a session.
		node->old_session = node->session;
		node->session = NULL;
		if (node->id == queue->north_sid)
			queue->north_sid = 0;
		if (node->id == queue->south_sid)
			queue->south_sid = 0;

		//node->id = 0;
		//queue->state = QUEUE_ITEM_FAILED;

		break;

	case SESSION_EVENT_IDLE:
		debugf("[handler_queue] idle event\n");
#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif
		break;

	case SESSION_EVENT_CMD:
		if ((id != QUEUE_EVENT_DIRECTORY_PHASE_2_SRC_DIRLIST) &&
			(id != QUEUE_EVENT_DIRECTORY_PHASE_2_DST_DIRLIST)) {
			debugf("[handler_queue] cmd event %d %d '%s'\n",
				   id, reply, line);
		} else {
			debugf("[handler_queue] dirlist event %d %d %p\n",
				   id, reply, line);
		}

		switch (id) {

		case QUEUE_EVENT_PHASE_1_SRC_CWD:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 250) {
				// Success
				// We want a way to know if we actually CD. Look for
				// "cached"? Kinda lame.
				if (!strstr(line, "cached"))
					if (queue->num_users) {
						char *name;
						name = misc_url_encode(queue->items->src.dirname);

						for (i = 0; i < queue->num_users; i++)
							if (queue->users[i] && queue->users[i]->handle)
								lion_printf(queue->users[i]->handle,
											"QS|QID=%u|SRCCWD=%s\r\n",
											queue->id,
											name);
						SAFE_FREE(name);
					}

			} else {
				// Initiate failure.
				queue_adderr_src(queue->items, line);
			}
			break;


		case QUEUE_EVENT_PHASE_1_DST_CWD:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 250) {
				// Success

				if (!strstr(line, "cached"))
					if (queue->num_users) {
						char *name;
						name = misc_url_encode(queue->items->dst.dirname);
						for (i = 0; i < queue->num_users; i++)
							if (queue->users[i] && queue->users[i]->handle)
								lion_printf(queue->users[i]->handle,
											"QS|QID=%u|DSTCWD=%s\r\n",
											queue->id,
											name);
						SAFE_FREE(name);
					}

			} else if ((reply >= 500)) {


				// Failed. Add error, but also try to create
				// the directory, if that succeeds, clear error.
				queue_adderr_dst(queue->items, line);

				debugf("QS|QID=%u|DSTMKD=%s\n",
					   queue->id,
					   queue->items->dst.dirname);

				// Try MKD, we fall-through down to trying
				// to send MKD.
				debugf("[handler] starting MKD with depth %d\n",
					   queue->mkd_depth);
				session_cmdq_newf(session,
								  0,
								  QUEUE_EVENT_PHASE_1_DST_MKD,
								  "MKD %s\r\n",
								  get_mkdname_by_depth(queue, 1));
			}

			break;


		case QUEUE_EVENT_PHASE_1_DST_MKD:
			if ((reply >= 200) && (reply <= 299)) {
				// Success

				if (queue->num_users) {
					char *name;
					name = misc_url_encode(get_mkdname_by_depth(queue, 0));

					for (i = 0; i < queue->num_users; i++)
						if (queue->users[i] && queue->users[i]->handle)
							lion_printf(queue->users[i]->handle,
										"QS|QID=%u|MKD=%s\r\n",
										queue->id,
										name);
					SAFE_FREE(name);
				}

				// Clear error.
				queue->items->dst_failure = 0;

				// Try CWD again.
				session_cwd(session, 0, QUEUE_EVENT_PHASE_1_DST_CWD2,
							get_mkdname_by_depth(queue, 0));

			} else if ((reply >= 500)) {

				// Failed

				queue_adderr_dst(queue->items, line);

			}
			break;

		case QUEUE_EVENT_PHASE_1_DST_CWD2:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 250) {

				// Success
				debugf("[handler] 2ND mkd of depth %d\n", queue->mkd_depth);
				if (queue->mkd_depth > 0) {
					session_cmdq_newf(session,
									  0,
									  QUEUE_EVENT_PHASE_1_DST_MKD,
									  "MKD %s\r\n",
									  get_mkdname_by_depth(queue, 1));
					break;
				}

			} else if ((reply >= 500)) {

				// Failure
				queue_adderr_dst(queue->items, line);

			}
			break;

		case QUEUE_EVENT_PHASE_2_SRC_SECURE:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 200) {
				// Success
				queue->secure_data = 1;
			} else {
				// Failure
				queue_adderr_src(queue->items, line);
			}
			break;

		case QUEUE_EVENT_PHASE_2_DST_SECURE:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 200) {
				// Success
				queue->secure_data = 1;
			} else {
				// Failure
				queue_adderr_dst(queue->items, line);
			}
			break;


		case QUEUE_EVENT_PHASE_2_DISABLE:
			// Do we care if we failed here? Disabling
			// CCSN/SSCN should never fail to disable.
			// We would need to disconnect session instead.
			// TODO
			break;

		case QUEUE_EVENT_PHASE_3_SRC_PROT:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 200) {
				// Success
			} else {
				// Failure
				queue_adderr_src(queue->items, line);
			}
			break;

		case QUEUE_EVENT_PHASE_3_DST_PROT:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 200) {
				// Success
			} else {
				// Failure
				queue_adderr_dst(queue->items, line);
			}
			break;



		case QUEUE_EVENT_PHASE_4_TYPE:
			break;

		case QUEUE_EVENT_PHASE_5_SRC_SIZE:
			if (reply == 213) {
				lion64_t newsize = 0;

				if (line &&
					sscanf((char *)line,
						   "213 %"PRIu64,
						   &newsize)
					== 1) {

					if (newsize != queue->items->src.size) {
						debugf("[handler] Updating src.size to %"PRIu64"\n",
							   newsize);
						queue->items->src.size = newsize;

					}

				} // scanf

			} // 213
			break;

		case QUEUE_EVENT_PHASE_5_DST_SIZE:
			if (reply == 213) {
				lion64_t newsize = 0;

				if (line &&
					sscanf((char *)line,
						   "213 %"PRIu64,
						   &newsize)
					== 1) {

					if (newsize != queue->items->dst.size) {
						debugf("[handler] Updating dst.size to %"PRIu64"\n",
							   newsize);
						queue->items->dst.size = newsize;

					}

				} // scanf

			} // 213
			break;


		case QUEUE_EVENT_PHASE_6_PRET:
			break;

		case QUEUE_EVENT_PHASE_7_PASV:
			if (reply == 227) {
				// 227 Entering Passive Mode (127,0,0,1,226,180)
				// 229 Entering Extended Passive Mode (|||58028|)
				// Look for () and safe inbetween. If that failed, we fail.
				char *r;

				if ((r = strchr(line, '('))) {

					line = &r[1];

					if ((r = strchr(line, ')'))) {

						*r = 0;

						SAFE_COPY(queue->pasv_line, line);

						return;
					}

				}

			}

			// Wrong reply, or couldn't parse, fail.
			if (queue->srcpasv)
				queue_adderr_src(queue->items, line);
			else
				queue_adderr_dst(queue->items, line);

			break;

		case QUEUE_EVENT_PHASE_8_PORT:
			if (reply == 200)
				break;

			if (!queue->srcpasv)
				queue_adderr_src(queue->items, line);
			else
				queue_adderr_dst(queue->items, line);

			break;

		case QUEUE_EVENT_PHASE_9_SRC_REST:
			if (reply == 350) {
				// REST ok
			} else {
				queue_adderr_src(queue->items, line);
			}
			break;

		case QUEUE_EVENT_PHASE_9_DST_REST:
			if (reply == 350) {
				// REST ok
			} else {
				queue_adderr_dst(queue->items, line);
			}
			break;

		case QUEUE_EVENT_PHASE_9_SRC_REST2:
		case QUEUE_EVENT_PHASE_9_DST_REST2:
			break;



		case QUEUE_EVENT_PHASE_10_STOR:

			if (reply == -1) {
				// LOG reply, in case we are looking for X-DUPE
				if (session->status & STATUS_ON_XDUPE) {
					// Look for X-DUPE lines, remember these are url
					// encoded.
					if (!strncasecmp("226-+X-DUPE%3A+", line, 15))
						queue_xdupe_item(queue, &line[15]);
				}
				break;
			}


			if ((reply == 150) ||  // Unix
				(reply == 125)) {  // Windows

				queue->items->src_transfer++;
				break;
			}
			queue_adderr_dst(queue->items, line);
			break;



		case QUEUE_EVENT_PHASE_11_RETR:
			if ((reply == 150) ||  // Unix
				(reply == 125)) {  // Windows

				queue->items->dst_transfer++;
				gettimeofday(&queue->xfr_start, NULL);
				break;
			}
			queue_adderr_src(queue->items, line);
			break;


		case QUEUE_EVENT_PHASE_11_ABOR:
			break;


		case QUEUE_EVENT_PHASE_12_SRC_226:
			queue->items->src_transfer++;
			if (reply == 226)
				break;
			queue_adderr_src(queue->items, line);
			break;

		case QUEUE_EVENT_PHASE_12_DST_226:

			queue->items->dst_transfer++;
			if (reply == 226)
				break;
			queue_adderr_dst(queue->items, line);
			break;




		case QUEUE_EVENT_PHASE_12_NOOP:
			break;











		case QUEUE_EVENT_DIRECTORY_PHASE_1_SRC_CWD:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 250) {
				// Success
				if (!strstr(line, "cached"))
					if (queue->num_users) {
						char *name;
						name = misc_url_encode(queue->items->src.fullpath);

						for (i = 0; i < queue->num_users; i++)
							if (queue->users[i] && queue->users[i]->handle)
								lion_printf(queue->users[i]->handle,
											"QS|QID=%u|SRCCWD=%s\r\n",
											queue->id,
											name);
						SAFE_FREE(name);
					}

			} else {
				// Initiate failure.
				queue_adderr_src(queue->items, line);
			}
			break;


		case QUEUE_EVENT_DIRECTORY_PHASE_1_DST_CWD:
			if ((reply >= 200) && (reply <= 299)) {
				//if (reply == 250) {

				// Success
				debugf("[handler] CLEARing depth**********\n");
				queue->mkd_depth = 0;

				if (!strstr(line, "cached"))
					if (queue->num_users) {
						char *name;
						name = misc_url_encode(queue->items->dst.fullpath);

						for (i = 0; i < queue->num_users; i++)
							if (queue->users[i] && queue->users[i]->handle)
								lion_printf(queue->users[i]->handle,
											"QS|QID=%u|DSTCWD=%s\r\n",
											queue->id,
											name);
						SAFE_FREE(name);
					}

			} else {
				// Initiate failure.
				queue_adderr_dst(queue->items, line);

				// Increase the depth that we've failed at, for MKDs.
				queue->mkd_depth++;
				debugf("[handler] INCreasing depth %d\n",queue->mkd_depth);
			}
			break;



		case QUEUE_EVENT_DIRECTORY_PHASE_2_SRC_DIRLIST:
			if (!line || !reply) {
				// No files. Empty dir. We have no FAILURE event in dirlist!!
				// FIXME!!
			}
			queue->items->src_transfer = 1;
			break;

		case QUEUE_EVENT_DIRECTORY_PHASE_2_DST_DIRLIST:
			if (!line || !reply) {
				// No files. Empty dir. We have no FAILURE event in dirlist!!
				// FIXME!!
			}
			queue->items->dst_transfer = 1;
			break;



		case QUEUE_EVENT_DIRECTORY_PHASE_2_DST_MKD:
			if ((reply >= 200) && (reply <= 299)) {
				// Success

				if (queue->num_users) {
					char *name;
					name = misc_url_encode(get_mkdname_by_depth(queue, 0));

					for (i = 0; i < queue->num_users; i++)
						if (queue->users[i] && queue->users[i]->handle)
							lion_printf(queue->users[i]->handle,
										"QS|QID=%u|MKD=%s\r\n",
										queue->id,
										name);
					SAFE_FREE(name);
				}

				// Clear error.
				queue->items->dst_failure = 0;

			} else if ((reply >= 500)) {

				// Failed

				queue_adderr_dst(queue->items, line);

			}
			break;


		} // switch EVENT_CMD (inner switch)


		break;


	}


}

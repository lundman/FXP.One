// $Id: session.c,v 1.50 2012/08/29 02:25:21 lundman Exp $
//
// Jorgen Lundman 5th February 2004.
//
//
//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#define HAVE_STDARG_H 1
#endif

#if HAVE_STDARG_H
#  include <stdarg.h>
#  define VA_START(a, f)        va_start(a, f)
#else
#  if HAVE_VARARGS_H
#    include <varargs.h>
#    define VA_START(a, f)      va_start(a)
#  endif
#endif
#ifndef VA_START
#  warning "no variadic api"
#endif

#include "version.h"

#include "debug.h"
#include "lion.h"
#include "misc.h"
#include "session.h"
#include "data.h"
#include "sites.h"
#include "file.h"
#include "local.h"

#include "engine.h"

extern int net_server_SSL;

/*
 *
 * session - this control all sessions needed to connect to remote sites.
 *

           USER .._.._.._..
            |              |
            | (optional, Q-M can operate alone)
            v              v
          Queue Mgr      Queue Mgr
            |              |
            |              | (optional second)
            v              v
          Session       Session
            |              |
            |              |
            v              v
          Remote FTP    Remote FTP

 * During a transer, queue is active.

           USER

      (optional, Q-M can operate alone)

          Queue Mgr -->  Queue Mgr
            |              |
            |              | (optional second)
            v              v
          Session       Session
            |              |
            |              |
            v              v
          Remote FTP    Remote FTP


 *
 * Sessions is responsible for the TCP connection to the remote FTPD, deal
 * with login, auth and so on, talks all the FTP protocol. Also in-charge of
 * keeping track of when it is "idle" or busy based on the command codes.
 *
 * Session also, in part, deals with data sessions, which is a subsection.
 * The idea is that you can pass session requests like:
 *
 * CONNECT: <site>.
 * SIZE: <file>
 * PASV:
 * PORT: <port>
 * RETR: <file>
 *
 * Higher commands such as "DIRLIST: <path>" are dealt with in Queue-Mgr.
 *
 *
 * We do not need a "is connected" method since if the site is lost, the
 * session simply reports it to Q-M and exits. We ask for a new session to
 * reconnect.
 *
 * API needs:
 *
 * session_create(site);      : Create a new session and connect remote host.
 * session_release();         : Disconnect and release
 * session_isidle();          : Is remote side ready for a new command?
 * session_command(cmd);      : Execute command.
 *
 * The session needs to inform the Q-M with the following async information.
 * session_lost               : Remote host closed, or lost, connection.
 * session_cmd_reply          : Executed command completed with code.
 * session_log                : Normal FTP output that user might want. [1]
 *
 * [1] Note that only the user is interested in this output, the QueueMgr never
 * needs it, with the one exception of the output from such commands as:
 * FEAT - as it is displayed human readable, not computer parsable. (Shame).
 * This is ok to, as we don't NEED it per say.
 *
 * Would be nice to have a command queue. Do we do that here, or in Q-M? That
 * queue is different to command-queue since it is User added items.
 *
 * We do have a command queue. Send the next command once the control session
 * is idle again.
 *
 */





//
// Take a callback handler, and a site to connect to.
// Setup a new session, ask lion to connect, and handle the lion
// events. Then relay appropriate events to the session handler.
//
session_t *session_new(session_handler_t handler, sites_t *site)
{
	session_t *result;


	if (!site || !handler)
		return NULL;



	// New node..
	result = (session_t *) malloc(sizeof(*result));
	if (!result) return NULL;

	memset(result, 0, sizeof(*result));


	// Setup their handler
	result->handler = handler;

	// Local transfer? or
	// Attempt connection

	if (!mystrccmp("<local>", site->host)) {
		result->handle = local_spawn(result);

		// We do not need a certificate to do SSL, except for
		// CCSN/SSCN
		result->features |= FEATURE_SSL;
        result->status   |= STATUS_ON_SSL;
		if (net_server_SSL) {
			result->features |= FEATURE_CCSN;
		}

	} else {
		result->handle = lion_connect(site->host, site->port,
									  site->iface, site->iport,
									  LION_FLAG_FULFILL,
									  result);


		// Implicit SSL? Do we have the key, get the setting (handles NULLs)
		if (str2yna(sites_find_extra(site, "IMPLICIT_TLS")) == YNA_YES) {
			lion_ssl_set(result->handle, LION_SSL_CLIENT);
            debugf("[session] Implicit SSL enabled.\n");
        }

	}

	// With fulfill we will never get NULL (really? even with out of memory?)

	lion_set_handler(result->handle, session_handler);


	// Set us up for the state engine.
	result->state = SESSION_ST_CONNECTING;

	result->site = site;

	// If we don't have the path, we should assume it is "/"
	if (mystrccmp("<local>", site->host)) {
		SAFE_COPY(result->pwd, "./");
	} else {
		SAFE_COPY(result->pwd, "/");
	}

	result->features |= FEATURE_SIZE; // Assume it has it.

	return result;
}




void session_free(session_t *session)
{

	//
	session->site = NULL;

	// Release filecache
	session_release_files(session);

	// Release file array holder finally.
	SAFE_FREE(session->files);
	session->files_allocated = 0;

	// Release cmd queue
	SAFE_FREE(session->cmdq);

	// Release paths
	SAFE_FREE(session->pwd);
	SAFE_FREE(session->saved_cwd);

	// Debug safety?
	memset(session, -1, sizeof(*session));

	// Release this session.
	SAFE_FREE(session);

}













session_handler_t session_set_handler(session_t *session,
									   session_handler_t handler)
{

	session->handler = handler;

	// If we've changed handler, and we are idle, we post the idle
	// event.
	if (session_isidle(session) &&
		!session->files_reply_id) {

		session->handler(session,
						 SESSION_EVENT_IDLE,
						 -1,
						 -1,
						 NULL);
	}

	return session->handler;

}

int session_isidle(session_t *session)
{

	if (!session) return 0;

	if (session->cmdq_size > 0)
		return 0;

	if (session->state < SESSION_ST_IDLE)
		return 0;

	if (session->isidle) return 1;
	return 0;
}





//
// userinput for all session sockets.
//
int session_handler(lion_t *handle, void *user_data,
					int status, int size, char *line)
{
	session_t *session = (session_t *) user_data;

	if (!user_data) {
		debugf("[session] handler called with NULL session: %d\n", status);
		return -1;
	}



	switch(status) {

	case LION_CONNECTION_CONNECTED:
	case LION_PIPE_RUNNING:
		session_set_state(session, SESSION_ST_CONNECTED, 0);
		debugf("[session] session connected.\n");
		break;

	case LION_CONNECTION_LOST:
	case LION_CONNECTION_CLOSED:
	case LION_PIPE_EXIT:
	case LION_PIPE_FAILED:
		// Call user handler with LOST.
		session->handle = NULL;
		session->handler(session, SESSION_EVENT_LOST, -1, 0, line);
		return 0;



	case LION_CONNECTION_SECURE_ENABLED:
	case LION_CONNECTION_SECURE_FAILED:
		// Just relay the event to the state handler.
		session_state_process(session, status, NULL);
		break;



	case LION_INPUT:

		if (debug_on > 1)
			if (line && session && session->site && session->site->name)
				debugf("*** '%s' INPUT '%s'\n",
					   session->site->name,
					   line);
		// We got input. Check if
		//
		// !isidle - check if it is a reply, or interim.
		// if we are in cmdq and want interim, OR, it is a reply
		// send the reply to the cmdq engine.
		// otherwise send it to state engine.
		// finally, send as log.

		// Attempt to check if this is a direct reply code, or interim.
		if (isdigit(line[0]) &&
			isdigit(line[1]) &&
			isdigit(line[2]) &&
			line[3] == ' ') {

			// Almost all the time we are actually waiting for the cmdq reply.
			// So one could feel the latter state_process relay is not required
			// but we sometimes wait for 220 (connect) and 226 (data transfer)
			// and we are not in cmdq. Nor idle? Hmm
			if (!session->isidle)
				session_cmdq_reply(session, atoi(line), line);
			else
				session_state_process(session, atoi(line), line);

		} else {

			// Quick hack for SSH detection.
			if (session && (session->state == SESSION_ST_AWAIT_220) &&
				line && (size > 4) &&
				(line[0] == 'S') &&
				(line[1] == 'S') &&
				(line[2] == 'H') &&
				(line[3] == '-')) {
				session_setclose(session, "FTP over SSH not supported. (FTP with SSL is supported)\n");
				break;
			}


			// not a direct reply, BUT the command might have asked for
			// log feedback then we send all to it.
			if (session && session->cmdq &&
				session->cmdq->flags & SESSION_CMDQ_FLAG_LOG) {

				line = misc_url_encode(line); // Needs to be FREEd

				if (session->cmdq->flags & SESSION_CMDQ_FLAG_INTERNAL) {

					if (session->cmdq->id > 0) {
						// We set the state here so that we send the LOG
						// message to the correct state. However, with one
						// exception, and that is RETR/STOR waiting on 226-
						// since that is bumped along when we see 150-
						if (session->state != SESSION_ST_RELAY_226)
							session_set_state(session, session->cmdq->id, 1);
					}
					session_state_process(session, -1, line);

					// If subid is set, we also send event to handler.
					if (session->cmdq->subid > 0)
						session->handler(session,
										 SESSION_EVENT_CMD,
										 session->cmdq->subid,
										 -1,
										 line);



				} else
					session->handler(session,
									 SESSION_EVENT_CMD,
									 session->cmdq->id,
									 -1,
									 line);

				SAFE_FREE(line);
				break;

			} // FLAG_LOG

		} // isdigit else

		// Send log output to caller.
		if (session && session->handler)
			session->handler(session, SESSION_EVENT_LOG, -1, -1, line);

		break;

	default:
		debugf("[session] unhandled event %p %d:%s\n",
			   handle, status, line ? line : "(null)");
		break;

	}

	return 0;

}



void session_cmdq_reply(session_t *session, int reply, char *line)
{
	cmdq_t tmp;
    int i;

    memset(&tmp, 0, sizeof(tmp)); // Clear *at least* tmp.cmd to NULL for the free

	if (session->cmdq_size <= 0) return;  // shouldn't happen


	// start of the cmdq must be the cmd (or we are out of sync :) )
	memcpy(&tmp, &session->cmdq[0], sizeof(tmp));

    session->cmdq[0].cmd = NULL; // Since "tmp" now holds pointer.
	session->cmdq_size--;

	// Keep any remaining
    for (i = 0; i < session->cmdq_size; i++)
        memcpy(&session->cmdq[i], &session->cmdq[i+1],
               sizeof(tmp));
        //if (session->cmdq_size) {
		//memcpy(&session->cmdq[0], &session->cmdq[1],
		//	   sizeof(tmp) * session->cmdq_size);

    // $1 = {cmd = 0x8e0f2f0 "\360\256\335\b\r\n", id = 17, subid = -1, flags = 3}

	debugf("[session] '%s' processing reply to '%4.4s' -> %d (%d CMD in queue)\n",
		   session && session->site ? session->site->name : "?",
		   tmp.cmd, reply, session->cmdq_size);


	// To whom do we send this event. Internal one, or caller one.
	if (tmp.flags & SESSION_CMDQ_FLAG_INTERNAL) {

		// Only set the state if it was requested.
		if (tmp.id > 0)
			session_set_state(session, tmp.id, 1);
		session_state_process(session, reply, line);

		// If subid is set, we also send event to handler.
		if (tmp.subid > 0)
			session->handler(session,
							 SESSION_EVENT_CMD,
							 tmp.subid,
							 reply,
							 line);

	} else
		session->handler(session,
						 SESSION_EVENT_CMD,
						 tmp.id,
						 reply,
						 line);

	// Free it.
	SAFE_FREE(tmp.cmd);

	// set idle here, if forcebusy is set we do nothing, but once
	// it cleared, isidle means we will finally send the next command.
	// This is the only place we set isidle, as we have received a reply.
	session->isidle = 1;
	session_cmdq_restart(session);

}


void session_cmdq_restart(session_t *session)
{

	debugf("[session] cmdq_restart: forcebusy %d isidle %d cmdq_size %d\n",
		   session->forcebusy,
		   session->isidle,
		   session->cmdq_size);

	if (!session->handle)
		return;

	// If we are busy, do nothing
	if (session->forcebusy) {
		return;
	}

	// if we are not idle, do nothing
	if (!session->isidle)
		return;


	// Do we have another? If so send it.
	if (session->cmdq_size > 0) {

		session->isidle = 0;

		if (session->cmdq[0].cmd) {
			lion_output(session->handle, session->cmdq[0].cmd,
						strlen(session->cmdq[0].cmd));

			if (strncmp("PASS ", session->cmdq[0].cmd, 5))
				debugf("[session] sending cmdq '%s'\n", session->cmdq[0].cmd);
		}
		return;
	}

	// Otherwise we really are idle.
	session->isidle = 1;

	session_set_state(session, SESSION_ST_IDLE, 1);

	if (session->handler)
		session->handler(session,
						 SESSION_EVENT_IDLE,
						 -1,
						 -1,
						 NULL);
}



//
// We do this simple assign as a function, so that would could put
// future check of (going from state, to new state) and have debug code
// in only one place.
//
void session_set_state(session_t *session, session_state_t state, int skip)
{
	int process = 0;

	debugf("[session] '%s' state %d => %d\n",
		   session->site ? session->site->name : "(null)",
		   session->state, state);

	if (session->state != state)
		process = 1;

	session->state = state;

	// Only call process if the state has changed.
	if (process && !skip)
		session_state_process(session, -1, NULL);

}






//
// Add a new command to the q.
//
static void session_cmdq_new_internal(session_t *session,
									  int flags,
									  int id,
									  int subid,
									  char const *line)
{

	if (!session->handle) return;

	// Do we have enough memory for this?
	if ( !session->cmdq ||
		 (session->cmdq_size >= session->cmdq_allocated)) { // Grow the list.
		cmdq_t *tmp_ptr;

		session->cmdq_allocated += SESSION_CMDQ_UNIT;

		tmp_ptr = (cmdq_t *) realloc(session->cmdq,
									 session->cmdq_allocated *
									 sizeof(cmdq_t));

		if (!tmp_ptr) {
			debugf("[session] out of memory growing cmdq\n");
			session_setclose(session, "out of memory");
			return;
		}

		session->cmdq = tmp_ptr;

	} // grow list.

	// Add this queue command.
	if (line)
		session->cmdq[session->cmdq_size].cmd   = strdup(line);  // XXX !
    else
        session->cmdq[session->cmdq_size].cmd   = NULL;
	session->cmdq[session->cmdq_size].id    = id;
	session->cmdq[session->cmdq_size].subid = subid;
	session->cmdq[session->cmdq_size].flags = flags;
	session->cmdq_size++;

	// If we are idle, send the data now.

    debugf("[session] adding cmdq %p with line '%s'\n",
           &session->cmdq[session->cmdq_size],
           line ? line : "(NULL)");

	session_cmdq_restart(session);

}




#if HAVE_STDARG_H
static void session_cmdq_newf_internal(session_t *session,
								int flags,
								int id,
								int subid,
								char const *fmt, ...)
#else
static void session_cmdq_newf(session, flags, id, subid, fmt, va_alist)
		  session_t *session;
		  int flags;
		  int id;
		  int subid,
		 char const *fmt;
		  va_dcl
#endif
{
	char buffer[1024];
	va_list ap;
	int result;


	if (!session) return;


	VA_START(ap, fmt);

	result = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	if (result <=0)
		strcpy(buffer, "FAILURE\r\n");


	session_cmdq_new_internal(session, flags, id, subid, buffer);

}


#if HAVE_STDARG_H
void session_cmdq_newf(session_t *session,
					   int flags,
					   int id,     // INTERNAL processing ID
					   char const *fmt, ...)
#else
void session_cmdq_newf(session, flags, id, fmt, va_alist)
	 session_t *session;
	 int flags;
	 int id;
	 char const *fmt;
     va_dcl
#endif
{
	static char buffer[1024];
	va_list ap;
	int result;


	if (!session) return;


	VA_START(ap, fmt);

	result = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	if (result <=0)
		strcpy(buffer, "FAILURE\r\n");


	session_cmdq_new_internal(session, flags, id, -1, buffer);

}




//
// Add a new command to the q.
//
void session_cmdq_new(session_t *session,
					  int flags,
					  int id,
					  char const *line)
{

	session_cmdq_new_internal(session, flags, id, -1, line);

}




void session_setclose(session_t *session, char *reason)
{

	debugf("[session] setclose '%s'\n", reason);

	// We want to close this, but doing so here, might not be a good thing.
	if (session->handle) {

		lion_set_userdata(session->handle, NULL);
		lion_disable_read(session->handle);
		lion_close(session->handle);
		session->handle = NULL;

		// Tell the caller.
		session->handler(session,
						 SESSION_EVENT_LOST,
						 -1,
						 -1,
						 reason);
	}

	session->handler = NULL;

	session_release_files(session);

	// We never release the session?
	debugf("[session] should release %p\n",
		   session);

	// FIXME, this WILL access free'd memory.
	//session_free(session);
}


void session_abor(session_t *session)
{

	// FIXME

}



//
// Initiate a dirlisting. Just add a command to the queue that starts it all.
//
void session_dirlist(session_t *session, char *args, int flags, int id)
{
	sites_t *site;

	site = session->site;


	session_release_files(session);



	// This here is a saved id, used only in dirlisting, so we know
	// where to send the actual dirlist event, including failures.
	session->files_reply_id = id;

	// Handle ASCII vs BINARY
	// If AUTO, we want ASCII
	// If NO, we want ASCII
	// If YES, we want BINARY

	if ((site->desired_type == YNA_AUTO)) { // && // if auto

		session_type(session, SESSION_CMDQ_FLAG_INTERNAL,
					 SESSION_ST_TYPE_REPLY,
					 0);
	}

	if ((site->desired_type != YNA_AUTO)) { // && // if not auto

		session_type(session, SESSION_CMDQ_FLAG_INTERNAL,
					 SESSION_ST_TYPE_REPLY,
					 site->desired_type == YNA_NO ? 0 : STATUS_ON_BINARY);
	}



	// Handle CCSN / SSCN
	// Technically not required as the RFC specifies that
	// LIST should always be processed as CCSN=Client (Off)
	session_ccsn(session, SESSION_CMDQ_FLAG_INTERNAL,
				 -1, // No event needed
				 0); // disable

	session_sscn(session, SESSION_CMDQ_FLAG_INTERNAL,
				 -1, // No event needed
				 0); // disable

	session_rest(session, SESSION_CMDQ_FLAG_INTERNAL,
				 -1, // No event needed
				 0); // 0, no rest.



	// Handle PROT level
	// local dirlists are done without SSL

	if (mystrccmp("<local>", site->host)) {

		if (session->features & FEATURE_SSL) {
			if ((site->data_TLS == YNA_AUTO)) { // &&   // if auto

				session_prot(session, SESSION_CMDQ_FLAG_INTERNAL,
							 SESSION_ST_PROT_REPLY,
							 STATUS_ON_PRIVACY);
			}

			if ((site->data_TLS != YNA_AUTO)) { // &&    // if not auto
				session_prot(session, SESSION_CMDQ_FLAG_INTERNAL,
							 SESSION_ST_PROT_REPLY,
							 site->data_TLS == YNA_NO ? 0 : STATUS_ON_PRIVACY);
			}
		}

	} else { // Local
		session_prot(session, SESSION_CMDQ_FLAG_INTERNAL,
					 SESSION_ST_PROT_REPLY,
					 0);
	}


	if ((site->pret == YNA_YES) ||
		((site->pret == YNA_AUTO) &&
		 (session_feat(session) & FEATURE_PRET))) {

		session_cmdq_new(session,
						 SESSION_CMDQ_FLAG_INTERNAL,
						 SESSION_ST_PRET_REPLY,
						 "PRET LIST\r\n");

	}


	// PASV vs PORT
	if (site->passive == YNA_NO) { // PORT only
		unsigned long addr;
		int port, dummy;

		// Open listening socket
		port = 0;  // FIXME - let users specify?
		session->data_handle = lion_listen(&port,
										   session->site->iface,
										   LION_FLAG_NONE, // Tell us now
										   session);

		if (!session->data_handle) {
			session_setclose(session, "Failed to open socket for PORT");
			return;
		}

		lion_set_handler(session->data_handle, data_handler);

		debugf("[session] listening...\n");

		// Get their remote peer, for port
		lion_getsockname(session->handle, &addr, &dummy);

		// send PORT command
		session_cmdq_newf(session,
						 SESSION_CMDQ_FLAG_INTERNAL,
						 SESSION_ST_PORT_REPLY,
						 "PORT %s\r\n",
						  lion_ftp_port(addr, port));

	} else {

		session_cmdq_new(session,
						 SESSION_CMDQ_FLAG_INTERNAL,
						 SESSION_ST_PASV_REPLY,
						 "PASV\r\n");

	}



	// send list
	session_cmdq_newf(session,
					  SESSION_CMDQ_FLAG_INTERNAL | flags,
					  SESSION_ST_DATA_TRANSFER,
					  "LIST %s\r\n",
					  args ? args : DEFAULT_LISTARGS);

	debugf("[session] sending 'LIST %s'\n",
					  args ? args : DEFAULT_LISTARGS);

}














//
//
// STATE PROCESSING
//
// Initial phase ->
// Connect phase -> connected ->
// if auth, start Auth Phase -> SSL successful ->
// Login Phase -> logged in ->
// Setup Phase -> info gathered ->
// IDLE
// Receive Command -> process
// Reply ->
// IDLE
//
// Dirlist Request ->
// Setup PASV/PORT ->
// Issue LIST ->
// Parse Data Channel ->
// Reply With List ->
// IDLE
//
void session_state_process(session_t *session, int reply, char *line)
{

	debugf("[session] state: %d reply %d\n", session->state, reply);

	switch(session->state) {



		//
		// INITIAL PHASE
		//


	case SESSION_ST_NONE:
		debugf("[session] ST_NONE reached?\n");
		break;


	case SESSION_ST_SENT_CMD:
		break; // Placeholder while we wait for a command to reply.



	case SESSION_ST_CONNECTING:
		if (reply == LION_CONNECTION_SECURE_ENABLED) {
			session->features |= FEATURE_SSL;
			session->status   |= STATUS_ON_SSL;

			debugf("[session] Implicit SSL successful.\n");
		}

		if (reply == LION_CONNECTION_SECURE_FAILED) {

			debugf("[session] Implicit SSL failed.\n");

			if (session->site->control_TLS == YNA_YES) {

				session_setclose(session,
								 "AUTH TLS/SSL Failed during challenge");
				break;
			}

			session->forcebusy = 0;
			session_cmdq_restart(session);
		}
		break; // un-used event.



	case SESSION_ST_CONNECTED:
		debugf("[session] connected.\n");

		session->isidle = 1;

		// Skip calling process right away since we are waiting for
		// a reply which can't come immediately.
		session_set_state(session, SESSION_ST_AWAIT_220, 1);
		break;


	case SESSION_ST_AWAIT_220:
		if (reply == 220) {
			debugf("[session] saw 220\n");

			// AUTH TLS, or login?
			// If TLS is YES, or AUTO, we attempt it.
			// Assuming we aren' already SSLed (Implicit SSL)
			if ((session->site->control_TLS != YNA_NO) &&
				!(session->status & STATUS_ON_SSL))
				session_set_state(session, SESSION_ST_TLS_START, 0);
			else
				session_set_state(session, SESSION_ST_LOGIN_START, 0);


		}
		break;







		//
		// AUTH TLS PHASE
		//





	case SESSION_ST_TLS_START:
		session_cmdq_newf(session,
						  SESSION_CMDQ_FLAG_INTERNAL,
						  -1,
						  "AUTH TLS\r\n",
						  session->site->user);
		// Change state so we ensure we don't end up here and queue another
		// cmdq
		session_set_state(session, SESSION_ST_TLS_PHASE_TLS, 1);
		break;


	case SESSION_ST_TLS_PHASE_TLS:
		if (reply == 234) {
			session->forcebusy = 1;  // don't send the next command!
			session_set_state(session, SESSION_ST_TLS_INIT, 0);
			debugf("[session] forcing busy\n");
			break;
		}

		if (reply > 0) { // 500 No Such Command etc

			session_cmdq_newf(session,
							  SESSION_CMDQ_FLAG_INTERNAL,
							  SESSION_ST_TLS_PHASE_SSL,
							  "AUTH SSL\r\n",
							  session->site->user);
			break;
		}
		break;


	case SESSION_ST_TLS_PHASE_SSL:
		if (reply == 234) {
			session_set_state(session, SESSION_ST_TLS_INIT, 0);
			break;
		}

		if (reply > 0) { // 500 No Such Command etc

			// If control_TLS is YES, then we fail entirely, if it
			// is Auto, it is ok that TLS failed, go to login.
			if (session->site->control_TLS == YNA_YES) {

				session_setclose(session, "AUTH TLS/SSL Failed");
				break;

			}

			session_set_state(session, SESSION_ST_LOGIN_START, 0);
			break;
		}
		break;


	case SESSION_ST_TLS_INIT:
		session_set_state(session, SESSION_ST_TLS_WAIT, 1);
		lion_ssl_set(session->handle, LION_SSL_CLIENT);
		break;


		// Fake event, called from lion handler for sessions.
		// pass it the lion event down as "reply".
	case SESSION_ST_TLS_WAIT:
		if (reply == LION_CONNECTION_SECURE_ENABLED) {
			session->features |= FEATURE_SSL;
			session->status   |= STATUS_ON_SSL;

			debugf("[session] SSL successful.\n");

			session_set_state(session, SESSION_ST_LOGIN_START, 0);

			session->forcebusy = 0;

			session_cmdq_restart(session);

			break;
		}

		if (reply == LION_CONNECTION_SECURE_FAILED) {

			if (session->site->control_TLS == YNA_YES) {

				session_setclose(session,
								 "AUTH TLS/SSL Failed during challenge");
				break;
			}

			session->forcebusy = 0;
			session_cmdq_restart(session);

		}
		break;


		//
		// LOGIN PHASE
		//






	case SESSION_ST_LOGIN_START:
		session_cmdq_newf(session,
						  SESSION_CMDQ_FLAG_INTERNAL,
						  SESSION_ST_LOGIN_SENT_USER,
						  "USER %s\r\n",
						  session->site->user);
		// Change state so we ensure we don't end up here and queue another
		// cmdq
		session_set_state(session, SESSION_ST_SENT_CMD, 1);
		break;

	case SESSION_ST_LOGIN_SENT_USER:
		if (reply == 331) {
			session_cmdq_newf(session,
							  SESSION_CMDQ_FLAG_INTERNAL,
							  SESSION_ST_LOGIN_SENT_PASS,
							  "PASS %s\r\n",
							  session->site->pass);
			break;
		}

		if (reply > 0)
			session_setclose(session, line);

		break;

	case SESSION_ST_LOGIN_SENT_PASS:

		if (reply == 230) {  // Login!
			session_set_state(session, SESSION_ST_MISC, 0);
			break;
		}
		if (reply > 0)  // We got a reply, but it wasn't 331. So error.
			session_setclose(session, line);

		break;




		//
		//
		// After login
		//
		//


		// FEAT
		// SYST
		// CLNT
		// (REST / REST)
	case SESSION_ST_MISC:
		session_set_state(session, SESSION_ST_IDLE, 1);


		if (session->features & FEATURE_SSL)
			session_cmdq_new(session,
							 SESSION_CMDQ_FLAG_INTERNAL,
							 SESSION_ST_REPLY_PBSZ,
							 "PBSZ 1024\r\n");


		session_cmdq_newf(session,
						  SESSION_CMDQ_FLAG_INTERNAL,
						  SESSION_ST_MISC_REPLY_SYST,
						  "SYST\r\n");

		session_cmdq_newf(session,
						  SESSION_CMDQ_FLAG_INTERNAL,
						  SESSION_ST_MISC_REPLY_CLNT,
						  "CLNT FXP.One engine %d.%d (#%d protocol %d.%d) \r\n",
						  VERSION_MAJOR,
						  VERSION_MINOR,
						  VERSION_BUILD,
						  PROTOCOL_MAJOR,
						  PROTOCOL_MINOR);

		// Feat is last, once we get its reply
		// we tell the client
		session_cmdq_newf(session,
						  SESSION_CMDQ_FLAG_INTERNAL|SESSION_CMDQ_FLAG_LOG,
						  SESSION_ST_MISC_REPLY_FEAT,
						  "FEAT\r\n");
		break;


	case SESSION_ST_REPLY_PBSZ:
		// Do we care?
		break;

	case SESSION_ST_MISC_REPLY_SYST:
		// Do we care?
		break;


	case SESSION_ST_MISC_REPLY_CLNT:
		// Do we care?
		break;

	case SESSION_ST_MISC_REPLY_FEAT:
		// Look for neat features?
		if (!line || !*line) break;


		if (line) debugf("[session] ftpd features: '%s'\n", line);

		// Since we URL encode the command, we compare with "+" instead of " "
		if (!line || !*line) break;
		if (!mystrccmp("+CCSN", line)) session->features |= FEATURE_CCSN;
		if (!mystrccmp("+SSCN", line)) session->features |= FEATURE_SSCN;
		if (!mystrccmp("+PRET", line)) session->features |= FEATURE_PRET;

		if (!strncasecmp("+XDUPE",line, 6)) {
			session->features |= FEATURE_XDUPE;
			if (strstr(line, "Enabled"))
				session->status |= STATUS_ON_XDUPE;
		}

		session_send_xdupe(session);

		break;

	case SESSION_ST_MISC_REPLY_XDUPE:
		if (reply == 200)
			session->status |= STATUS_ON_XDUPE;
		break;

	case SESSION_ST_TYPE_REPLY:
		if (reply == 200) {
			// All ok!
			session->status &= ~STATUS_ON_BINARY;
			break;
		}
		if (reply > 0) { // some other reply
			debugf("[session] WARNING: Could not set TYPE level!?\n");
			//session->site->current_type = -1;  //try again
		}
		break;

	case SESSION_ST_PROT_REPLY:
		if (reply == 200) {
			// All ok!
			break;
		}
		if (reply > 0) { // some other reply
			debugf("[session] WARNING: Could not set PROT level!\n");
			//session->site->current_prot = -1;  //try again

			// Data secure forced? If so fail right now.
			if (session->site->data_TLS == YNA_YES)
				session_setclose(session, "Could not set TLS PRIVACY when forced");

		}
		break;



	case SESSION_ST_PRET_REPLY:
		break;




	case SESSION_ST_PASV_REPLY:
		// 227 PASV reply
		// 229 EPSV reply
		// 425 temp failure
		// 426 temp failure
		if (reply == 227) {
			// 227 Entering Passive Mode (1,1,1,6,4,8)
			if (!lion_ftp_pasv(line,
							   &session->data_host,
							   &session->data_port)) {
				session_setclose(session, "PASV reply parse error");
				break;
			}

			debugf("[session] data connection...%s:%d\n",
				   lion_ntoa(session->data_host), session->data_port);

			if (session->data_handle) debugf("[session] warning data_handle is not NULL %p\n",
									 session->data_handle);

			// Ask lion to connect it for us.
			session->data_handle = lion_connect(
												lion_ntoa(session->data_host),
												session->data_port,
												session->site->iface,
												session->site->iport,
												LION_FLAG_FULFILL,
												session);

			lion_disable_read(session->data_handle);

			lion_set_handler(session->data_handle, data_handler);

			debugf("[session] connecting...\n");

			// Enable SSL?
			if (session->status & STATUS_ON_PRIVACY) {
                debugf("[session] requesting SSL as CLIENT\n");
				lion_ssl_set(session->data_handle, LION_SSL_CLIENT);
            }

			break;
		}

		if (reply > 0) { // FIXME - softer failure.
			session_setclose(session, line);
		}
		break;

	case SESSION_ST_PORT_REPLY:
		if (reply == 200) {

			break;
		}
		session_setclose(session, line);
		break;

	case SESSION_ST_DATA_TRANSFER:
		if (reply == 150) {
			debugf("[session] 150\n");
			session_set_state(session, SESSION_ST_DATA_FINISH, 1);
			session->forcebusy = 1; // We are not idle until we see a 226
			// It is entirely possible that data_handle is NULL here.
			// See .core 2005/11/22
			if (!session->data_handle) {
				//				session_setclose(session, "DATA handle is NULL");
				debugf("[session] DATA is NULL! Ignoring though..\n");
				break;
			}

			lion_enable_read(session->data_handle);
			break;
		}

		if (reply > 0) {
			debugf("[session] data transfer failed to start: %s\n", line);
			session_setclose(session, line); // FIXME - soft failure
			break;
		}
		break;


	case SESSION_ST_DATA_FINISH:
		// Should we also make sure that the data connection has finished?
		debugf("[data_finish] data isconnected %d\n",
			   lion_isconnected(session->data_handle));

		if (reply == 226) {
			debugf("[session] 226\n");
			session->forcebusy = 0;
			session_set_state(session, SESSION_ST_IDLE, 1);
			session_cmdq_restart(session);
			break;
		}
		if (reply > 0) {
			debugf("[session] data transfer failed part way\n");
			session_setclose(session, line); // FIXME - soft failure
			break;
		}
		break;


	case SESSION_ST_IDLE:
		break;


		//
		//
		// The following events are special, they also relay an event to
		// the API, using subid.
		//
		//

	case SESSION_ST_RELAY_TYPE_ASCII:
		if (reply == 200)
			session->status &= ~STATUS_ON_BINARY;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_TYPE_BINARY:
		if (reply == 200)
			session->status |= STATUS_ON_BINARY;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_CCSN_OFF:
		if (reply == 200)
			session->status &= ~STATUS_ON_CCSN;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_CCSN_ON:
		if (reply == 200)
			session->status |= STATUS_ON_CCSN;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_SSCN_OFF:
		if (reply == 200)
			session->status &= ~STATUS_ON_SSCN;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_SSCN_ON:
		if (reply == 200)
			session->status |= STATUS_ON_SSCN;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_PRIVACY_OFF:
		if (reply == 200)
			session->status &= ~STATUS_ON_PRIVACY;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_PRIVACY_ON:
		if (reply == 200)
			session->status |= STATUS_ON_PRIVACY;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_CWD:
		if (reply == 250) {

			if (session->saved_cwd)
				SAFE_COPY(session->pwd, session->saved_cwd);

			// If we CWD, we should release the dircache.
			// Maybe we should check for "CD ." situations?
			session_release_files(session);

			session_cmdq_new(session,
							 SESSION_CMDQ_FLAG_INTERNAL,
							 SESSION_ST_PWD_INTERNAL,
							 "PWD\r\n");
			break;

		} else {
			// CWD failed, so we know that "saved_cwd" is definitely
			// wrong, hence, wipe it. ->pwd, if set, is still valid.
			SAFE_FREE(session->saved_cwd);

		}
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_SIZE:
		if (reply == 500) { // No such command
			session->features &= ~FEATURE_SIZE;
			break;
		}
		// Has SIZE command, remember it.
		session->status |= STATUS_ON_SIZE;
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;


	case SESSION_ST_RELAY_REST_ON:
		if (reply == 350) { // No such command
			session->status |= STATUS_ON_REST;
			break;
		}
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_REST_OFF:
		if (reply == 350) { // No such command
			session->status &= ~STATUS_ON_REST;
			break;
		}
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;

	case SESSION_ST_RELAY_STOR:
	case SESSION_ST_RELAY_RETR:
		if ((reply == 150) ||
			(reply == 130) ||
			(reply == 125)) {
			session->forcebusy = 1; // We are not idle until we see a 226
			session_set_state(session, SESSION_ST_RELAY_226, 1);
		}
		break;

	case SESSION_ST_RELAY_226:
		if (reply > 0) {
			session->forcebusy = 0;
			session->handler(session,
							 SESSION_EVENT_CMD,
							 session->files_reply_id,
							 reply,
							 line);
			session->files_reply_id = 0;
			session_set_state(session, SESSION_ST_IDLE, 1);
		}
		break;


	case SESSION_ST_PWD_INTERNAL:
		// Attempt to fetch out the pwd, and assign it if we can.
		// Ok, we should not call this function from here, since
		// session is lower than handler. It needs to move into a
		// support function, that both can call.
		session_pwd_reply(session, reply, line);
		session_set_state(session, SESSION_ST_IDLE, 1);
		break;


	default:
		debugf("[session] help! un-accounted state reached: %d\n",
			   session->state);
		break;
	}

}





void session_release_files(session_t *session)
{
	int i;

	debugf("[session] releasing file cache: '%s'\n",
		   session->pwd);

	for (i = 0; i < session->files_used; i++)
		file_free(session->files[i]);

	// Do we want to free the main ptr here? If they dirlist again,
	// we can just use the old one.
	//SAFE_FREE(session->files);
	//session->files_allocated = 0;

	session->files_used = 0;

}



void session_parse(session_t *session, char *line)
{
	file_t *newd, **newg;

	newd = file_parse(line, session->cmdq &&
					  session->cmdq[0].flags & SESSION_CMDQ_FLAG_RAWLIST);

	if (!newd) return;

	// Add in pwd to complete dirname and fullpath
	file_makepath(newd, session->pwd);


	// New file entry, squeeze it in.
	// _allocated, _size, _ptr

	if (session->files_used >= session->files_allocated) {

		newg = (file_t **)realloc(session->files,
								 sizeof(file_t *) * (session->files_allocated + SESSION_FILES_INCREMENT));

		if (!newg) {
			file_free(newd);
			return;
			// failed, we will miss this file, but we will try
			// again too.
		}

		session->files = newg;
		session->files_allocated += SESSION_FILES_INCREMENT;
	}

	// Tag on at the end.

	// Its fid is its position, but lets put it in the struct too, to be
	// nice.
	newd->fid = session->files_used + 1;
	session->files[ session->files_used++ ] = newd;

}


file_t *session_getfid(session_t *session, unsigned int fid)
{
	int i;

	debugf("[session] looking for fid %u\n", fid);

	for (i = 0; i < session->files_used; i++)
		if (session->files[i]->fid == fid) {

			debugf("[session] returning with '%s' '%s'/'%s'\n",
				   session->files[i]->fullpath,
				   session->files[i]->dirname,
				   session->files[i]->name);

			return session->files[i];
		}

	return NULL;
}



file_t *session_getname(session_t *session, char *name)
{
	int i;

	for (i = 0; i < session->files_used; i++)
		if (!mystrccmp(session->files[i]->name, name)) {

			debugf("[session] returning with '%s' '%s'/'%s'\n",
				   session->files[i]->fullpath,
				   session->files[i]->dirname,
				   session->files[i]->name);

			return session->files[i];
		}

	return NULL;
}





//
// Set TYPE of the session. We cache results and remember then so that
// we do not send the command again if we are already at this state.
// Of course, as a warning to the API, if you send the command using
// the QUOTE (raw) method, we have no way to track it.
//
void session_type(session_t *session, int flags, int id,
				  unsigned int want)
{

	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		if (want && (session->status & STATUS_ON_BINARY)) {
			// want BINARY (0 = ascii) and we are in binary

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Type set to I. (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

		if (!want && !(session->status & STATUS_ON_BINARY)) {
			// want ASCII and we are in ASCII

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Type set to A. (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

	} // force

	// We need to send the command

	if (!want) {

		// ASCII
		session_cmdq_new_internal(session,
								   SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_TYPE_ASCII,
								   id,
								   "TYPE A\r\n");
	} else {

		// BINARY
		session_cmdq_new_internal(session,
								  SESSION_CMDQ_FLAG_INTERNAL,
								  SESSION_ST_RELAY_TYPE_BINARY,
								  id,
								  "TYPE I\r\n");

	}


}



//
// Set CCSN of the session. We cache results and remember then so that
// we do not send the command again if we are already at this state.
// Of course, as a warning to the API, if you send the command using
// the QUOTE (raw) method, we have no way to track it.
//
void session_ccsn(session_t *session, int flags, int id,
				  unsigned int want)
{

	// Do we want to check that this host has FEATURE_CCSN ?
	// If USE_CCSN is AUTO and FEATURE_CCSN, or YES, we should send command.
	// Otherwise, we do not.

	// Send IDLE?
	if (session->site->data_TLS == YNA_NO)
		return;

	if ((session->site->data_TLS == YNA_AUTO) &&
		!(session->features & FEATURE_CCSN))
		return;


	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		if (want && (session->status & STATUS_ON_CCSN)) {
			// want CCSN ON and we are have it ON.

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Client Mode (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

		if (!want && !(session->status & STATUS_ON_CCSN)) {
			// want CCSN OFF and we are have it OFF.

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Server Mode (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

	} // force

	// We need to send the command

	if (!want) {

		// OFF
		session_cmdq_new_internal(session,
								   SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_CCSN_OFF,
								   id,
								   "CCSN OFF (cached)\r\n");
	} else {

		// ON
		session_cmdq_new_internal(session,
								  SESSION_CMDQ_FLAG_INTERNAL,
								  SESSION_ST_RELAY_CCSN_ON,
								  id,
								  "CCSN ON (cached)\r\n");

	}


}


//
// Set SSCN of the session. We cache results and remember then so that
// we do not send the command again if we are already at this state.
// Of course, as a warning to the API, if you send the command using
// the QUOTE (raw) method, we have no way to track it.
//
void session_sscn(session_t *session, int flags, int id,
				  unsigned int want)
{

	// Do we want to check that this host has FEATURE_SSCN ?
	// If USE_SSCN is AUTO and FEATURE_SSCN, or YES, we should send command.
	// Otherwise, we do not.

	// Send IDLE?
	if (session->site->data_TLS == YNA_NO)
		return;

	if ((session->site->data_TLS == YNA_AUTO) &&
		!(session->features & FEATURE_SSCN))
		return;


	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		if (want && (session->status & STATUS_ON_SSCN)) {
			// want SSCN ON and we are have it ON.

			// Only send user event if there is a user
			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Client Mode (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

		if (!want && !(session->status & STATUS_ON_SSCN)) {
			// want SSCN OFF and we are have it OFF.

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 Server Mode (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

	} // force

	// We need to send the command

	if (!want) {

		// OFF
		session_cmdq_new_internal(session,
								   SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_SSCN_OFF,
								   id,
								   "SSCN OFF\r\n");
	} else {

		// ON
		session_cmdq_new_internal(session,
								  SESSION_CMDQ_FLAG_INTERNAL,
								  SESSION_ST_RELAY_SSCN_ON,
								  id,
								  "SSCN ON\r\n");

	}


}




//
// Set PROT of the session. We cache results and remember then so that
// we do not send the command again if we are already at this state.
// Of course, as a warning to the API, if you send the command using
// the QUOTE (raw) method, we have no way to track it.
//
void session_prot(session_t *session, int flags, int id,
				  unsigned int want)
{




	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		if (want && (session->status & STATUS_ON_PRIVACY)) {
			// want PRIVACY ON and we are have it ON.

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 OK (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

		if (!want && !(session->status & STATUS_ON_PRIVACY)) {
			// want PRIVACY OFF and we are have it OFF.

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 200,
								 "200 OK (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

	} // force

	// We need to send the command

	if (!want) {

		// OFF
		session_cmdq_new_internal(session,
								   SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_PRIVACY_OFF,
								   id,
								   "PROT C\r\n");
	} else {

		// ON
		session_cmdq_new_internal(session,
								  SESSION_CMDQ_FLAG_INTERNAL,
								  SESSION_ST_RELAY_PRIVACY_ON,
								  id,
								  "PROT P\r\n");

	}


}


//
// Set CWD of the session. We cache results and remember then so that
// we do not send the command again if we are already at this state.
// Of course, as a warning to the API, if you send the command using
// the QUOTE (raw) method, we have no way to track it.
//
void session_cwd(session_t *session, int flags, int id,
				 char *want)
{

    if (!want) {
        debugf("[session] CWD with NULL path\n");
        return;
    }

	debugf("[session] CWD: '%s' to '%s' for id %d, handler %p\n",
		   session->site->name,
		   want,
		   id,
		   session->handler);

	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		// Are we already in this directory?
		// Do we use case-sensitive match, or, insensitive?
		if ((*want == '/') &&
			((session->pwd && !strcmp(want, session->pwd)) ||
			(session->saved_cwd && !strcmp(want, session->saved_cwd)))) {

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 250,
								 "250 CWD command successful. (cached)");

			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);

			return;
		}

	} // force


	// We save the "cwd" we want now, so that if PWD fails, we
	// will set the ->pwd to saved_cwd, instead of pwd reply.
	// This happens if FTP has no-see directories set up.
	SAFE_COPY(session->saved_cwd, want);

	// We need to send the command
	session_cmdq_newf_internal(session,
							   flags|SESSION_CMDQ_FLAG_INTERNAL,
							   SESSION_ST_RELAY_CWD,
							   id,
							   "CWD %s\r\n",
							   want);

}





//
// Send SIZE <file>  to server. We wrap it here because some servers do
// not accept the SIZE command, so we try to speed things up by remembering
// that lack of capabilities.
//
void session_size(session_t *session, int flags, int id,
				 char *want)
{

	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {


		if (!session->features & FEATURE_SIZE) {

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 500,
								 "500 'SIZE': command not understood. (cached)");
			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);
			return;
		}

	} // force


	// If this is the first time we are sending SIZE, we also
	// take the event internally so we can remember if site has it.

	if (!(session->status & STATUS_ON_SIZE)) {

		session_cmdq_newf_internal(session,
								   flags|SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_SIZE,
								   id,
								   "SIZE %s\r\n",
								   want);
		return;
	}

	// Just send the command
	session_cmdq_newf(session,
					  flags,
					  id,
					  "SIZE %s\r\n",
					  want);


}



//
// Send REST <pos> to server. We remember if we've sent a non-zero or
// zero value. So that the above layer can just REST=0 without needing to
// remember if it was sent.
//
void session_rest(session_t *session, int flags, int id,
				  lion64_t want)
{

	// if not forced, and we are already in this state, we need
	// not send anything.
	if (!(flags & SESSION_CMDQ_FLAG_FORCE)) {

		// want 0, and we last sent REST 0
		if (!want && !(session->status & STATUS_ON_REST)) {

			if ((id > 0) && session->handler)
				session->handler(session,
								 SESSION_EVENT_CMD,
								 id,
								 350,
								 "350 Restarting at 0. Send STORE or RETRIEVE to initiate transfer. (cached)");
			// Also send fake IDLE event, we fake this
			// by setting handler to the same handler again.
			session_set_handler(session, session->handler);
			return;

		}


	}

	if (want)
		session_cmdq_newf_internal(session,
								   flags|SESSION_CMDQ_FLAG_INTERNAL,
								   SESSION_ST_RELAY_REST_ON,
								   id,
								   "REST %"PRIu64"\r\n",
								   want);
	else
		session_cmdq_new_internal(session,
								  flags|SESSION_CMDQ_FLAG_INTERNAL,
								  SESSION_ST_RELAY_REST_OFF,
								  id,
								  "REST 0\r\n");

}








unsigned int session_feat(session_t *session)
{
	if (!session) return 0;

	return session->features;

}





// PWD reply we try to be nice and parse out the directory we are in, if
// at all possible.
//
char *session_pwd_reply(session_t *node, int reply, char *line)
{
	char *copy = NULL, *ar, *path;

	if (line && *line) {

		SAFE_COPY(copy, line);

		if (copy) {

			ar = strchr(copy, '"');

			if (ar) {

				path = misc_digtoken(&ar, "\"");

				if (path && *path) {

					SAFE_COPY(node->pwd, path);

					SAFE_FREE(copy);
					return node->pwd;

				}

			}

			SAFE_FREE(copy);

		} // if copy

	}

	// Couldn't PWD - but the CWD may have worked before, so then we
	// should trust that.
	if (node->saved_cwd)
		SAFE_COPY(node->pwd, node->saved_cwd);

	return node->pwd;
}





//
// Send STOR to remote site. These are done in here as they are
// the only commands that are not challenge->response. They return
// two responses. (Start, and Finish, transfer).
//
void session_stor(session_t *session, int flags, int start_event,
				  int end_event, char *file)
{

	session->files_reply_id = end_event;

	session_cmdq_newf_internal(session,
							   flags|SESSION_CMDQ_FLAG_INTERNAL,
							   SESSION_ST_RELAY_STOR,
							   start_event,
							   "STOR %s\r\n",
							   file);

	//session_cmdq_new_internal(session,
	//					  flags|SESSION_CMDQ_FLAG_INTERNAL,
	///					  SESSION_ST_RELAY_226,
	//					  end_event,
	//					  NULL);

}

//
// Send RETR to remote site. These are done in here as they are
// the only commands that are not challenge->response. They return
// two responses. (Start, and Finish, transfer).
//
void session_retr(session_t *session, int flags, int start_event,
				  int end_event, char *file)
{

	session->files_reply_id = end_event;

	session_cmdq_newf_internal(session,
							   flags|SESSION_CMDQ_FLAG_INTERNAL,
							   SESSION_ST_RELAY_RETR,
							   start_event,
							   "RETR %s\r\n",
							   file);

	//session_cmdq_new_internal(session,
	//					  flags|SESSION_CMDQ_FLAG_INTERNAL,
	//					  SESSION_ST_RELAY_226,
	//					  end_event,
	//					  NULL);

}




// Determine if we should send X-DUPE command. If we see "Enabled"
// we need not to. Otherwise, check "USE_XDUPE" key pair, if it
// is "YES" always send, if it is "AUTO" send if we saw FEAT.
void session_send_xdupe(session_t *session)
{
	char *value;
	yesnoauto_t xdupe;

	value = sites_find_extra(session->site, "USE_XDUPE");

	// Convert that to yesnoauto, and yes, it handles NULL.
	xdupe = str2yna(value);

	if ((xdupe == YNA_YES) ||
		((session->features & FEATURE_XDUPE) &&
		 !(session->status & STATUS_ON_XDUPE))) {

		// Lets turn on X-DUPE then.
        //session_cmdq_newf(session, flags, id, subid, fmt, va_alist)

		session_cmdq_new(session,
						  SESSION_CMDQ_FLAG_INTERNAL,
						  SESSION_ST_MISC_REPLY_XDUPE,
						  "SITE XDUPE 3\r\n");
		return;
	}

	// If forced off, make sure status is unset, then we do not
	// listen to the X-DUPE commands.
	if (xdupe == YNA_NO)
		session->status &= ~STATUS_ON_XDUPE;
}

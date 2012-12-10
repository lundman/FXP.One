// $Id: data.c,v 1.7 2011/11/01 04:55:14 lundman Exp $
//
// Jorgen Lundman 5th February 2004.
//
//
//


#include <stdio.h>

#include "lion.h"

#include "debug.h"
#include "data.h"
#include "session.h"
#include "file.h"





int data_handler(lion_t *handle, void *user_data, int status, int size,
				 char *line)
{
	session_t *session = (session_t *) user_data;


#ifdef DEBUG_VERBOSE
	debugf("[data] handler called %p -> event %d\n",
		   handle, status);
#endif


	if (!session) {

		debugf("[data] handler called with NULL session!\n");
		return -1;

	}

	switch(status) {

	case LION_CONNECTION_CONNECTED:
		debugf("[data] '%s' connected\n", session->site->name);
		break;

	case LION_CONNECTION_NEW: // PORT, incoming connection
		{
			unsigned long addr;
			int port;

			session->data_handle = lion_accept( session->data_handle,
												1,
												LION_FLAG_FULFILL,
												session,
												&addr,
												&port);
			lion_set_handler(session->data_handle, data_handler);

			// Disable read before SSL negotiations don7t work too well.
			//lion_disable_read(session->data_handle);

			debugf("[data] '%s' PORT connection from %s:%d\n",
				   session->site->name,
				   lion_ntoa(addr), port);

			// Enable SSL?
			if (session->status & STATUS_ON_PRIVACY) {
				lion_ssl_set(session->data_handle, LION_SSL_CLIENT);
				debugf("[data] Requesting SSL as CLIENT\n");
			}

		}
		break;


	case LION_CONNECTION_LOST:
		session_setclose(session, "data connection lost");
		session->data_handle = NULL;
		break;

	case LION_CONNECTION_CLOSED:
		debugf("[data] closed gracefully.\n");

		// Pass appilcation the file list.
		debugf("[data] handing off event\n");
        if (session->handler)
            session->handler(session,
                             SESSION_EVENT_CMD,
                             session->files_reply_id,
                             session->files_used,
                             (char *)session->files );
		session->data_handle = NULL;
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf("[data] SSL negotiated OK\n");
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("[data] SSL negotiation FAILED\n");
		if (session->site->data_TLS == YNA_YES)
			session_setclose(session,
							 "Secure DATA failed -- aborting per user option");
		break;

	case LION_INPUT:
#ifdef DEBUG_VERBOSE
		debugf("  [data] '%s'\n", line);
#endif
		session_parse(session, line);
		break;

	default:
		debugf("[data] unhandled event %p %d:%s\n",
			   handle, status, line ? line : "(null)");
		break;

	}

	return 0;

}




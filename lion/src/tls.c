
/*-
 *
 * New BSD License 2006
 *
 * Copyright (c) 2006, Jorgen Lundman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1 Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2 Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 3 Neither the name of the stuff nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// $Id: tls.c,v 1.13 2012/08/28 00:31:18 lundman Exp $
// SSL/TLS Support Functions
// Jorgen Lundman October 17th, 2002.

#if HAVE_CONFIG_H
#include <config.h>
#endif


#ifdef WITH_SSL

#include <stdio.h>

// If you don't have these, have you installed openssl?
#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>   // SSL below includes windows, that includes old winsock, so we do it first
#endif

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <string.h>

#include "connections.h"

#include "lion.h"
#include "tls.h"
#include "sockets.h"


//#define DEBUG
//#define DEBUG_VERBOSE
#undef DEBUG

__RCSID("$LiON: lundman/lion/src/tls.c,v 1.13 2012/08/28 00:31:18 lundman Exp $");




static char cfg_tlsciphers[] =
"RC4-SHA:RC4-MD5:DHE-DSS-RC4-SHA:DES-CBC3-SHA:DES-CBC3-MD5:EDH-RSA-DES-CBC3-SHA:EDH-DSS-DES-CBC3-SHA";


// Master TLS context. Threads need to be able to access this! Or should children call init again?
//THREAD_SAFE
	static SSL_CTX *tls_ctx = NULL;

// internal variables to remember which has been initialised
int net_client_SSL = 1;
int net_server_SSL = 1;

char *ssl_tlsciphers = NULL;
char *ssl_tlsrsafile = NULL;
char *ssl_egdsocket = NULL;


int tls_init( void )
{
	int egdbytes, status;


	// Do we even want SSL on?
	if (!net_client_SSL &&
		!net_server_SSL)
		return 0;

	if (!ssl_tlsciphers)
		ssl_tlsciphers = strdup( cfg_tlsciphers );
	if (!ssl_egdsocket)
		ssl_egdsocket = strdup( "/var/run/egd-pool" );
	if (!ssl_tlsrsafile)
		ssl_tlsrsafile = strdup( "lion.pem" );

#ifdef DEBUG
	printf("Using cipher list: %s\n", ssl_tlsciphers);
#endif

	SSL_load_error_strings();                /* readable error messages */
	SSL_library_init();                      /* initialize library */

	tls_ctx = SSL_CTX_new(SSLv23_method());
	
	// Did we get SSL?

	if (!tls_ctx) {

		net_server_SSL = 0;
		net_client_SSL = 0;

		printf("SSL_CTX_new() %s\r\n",
			   (char *)ERR_error_string(ERR_get_error(), NULL));

		tls_free();

		return -1;
	}

	// Do we have sufficient random?

	if (RAND_status() != 1) {

		if ( (egdbytes = RAND_egd(ssl_egdsocket)) == -1 ) {

			net_server_SSL = 0;
			net_client_SSL = 0;

			printf("ssl_init: RAND_egd failed\r\n");

			tls_free();

			return -2;
		}

		if (RAND_status() != 1) {

			net_server_SSL = 0;
			net_client_SSL = 0;

			printf("ssl_init: System without entropy source, see TLS.ReadMeFirst\r\n");

			tls_free();

			return -3;
		}
	}


	// Set some options
	//SSL_CTX_set_options(tls_ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(tls_ctx, SSL_OP_NO_SSLv2);
#ifdef SSL_OP_NO_COMPRESSION
	SSL_CTX_set_options(tls_ctx, SSL_OP_NO_COMPRESSION);
#endif
	//SSL_CTX_set_options(tls_ctx, SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);
	//SSL_CTX_set_options(tls_ctx, SSL_OP_NO_TLSv1);
	SSL_CTX_set_options(tls_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
	//SSL_CTX_set_options(tls_ctx, SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION|SSL_OP_LEGACY_SERVER_CONNECT);

	SSL_CTX_set_default_verify_paths(tls_ctx);



	SSL_CTX_set_session_cache_mode(tls_ctx, SSL_SESS_CACHE_CLIENT);
	SSL_CTX_set_session_id_context(tls_ctx, (const unsigned char *) "1", 1);

	// That's sufficient for being a client (server communications)
	// but we also need to load up certificates etc to attempt to
	// allow clients to SSL with us.


	if (net_server_SSL) { // SSL is enabled for clients


		status = SSL_CTX_use_certificate_file(tls_ctx, ssl_tlsrsafile,
											  X509_FILETYPE_PEM);

		if (status <= 0) {
			printf("SSL_CTX_use_certificate_file(%s) %s : %s\n", "",
				   (char *)ERR_error_string(ERR_get_error(), NULL),
				   ssl_tlsrsafile);

			printf("Server SSL AUTH disabled (no certificate).\n");

			net_server_SSL = 0;


		} else {

			status = SSL_CTX_use_PrivateKey_file(tls_ctx,ssl_tlsrsafile,
												 X509_FILETYPE_PEM);

			if (status <= 0) {
				printf("SSL_CTX_use_PrivateKey_file(%s) %s\n", "",
					   (char *)ERR_error_string(ERR_get_error(), NULL));
				printf("Server SSL AUTH disabled.\n");
				net_server_SSL = 0;
			}

		}

	}



	return 0;

}


void tls_free(void)
{

	if (tls_ctx) {

#ifdef DEBUG
		printf("SSL Released.\n");
#endif

		SSL_CTX_free(tls_ctx);
		tls_ctx = NULL;

	}

	net_server_SSL = 0;
	net_client_SSL = 0;

}


void tls_close( connection_t *node )
{

	if (node->trace)
		fprintf(trace_file, "%p: tls_close (%p)\n", node, node->ctx);

	if (node->ctx) {
#ifdef DEBUG
		printf("Releasing SSL socket\n");
#endif
        SSL_shutdown(node->ctx);
		SSL_free( node->ctx );
		node->ctx = NULL;
	}

	node->use_ssl = 0;


}


int tls_auth( connection_t *node )
{

#ifdef DEBUG
	printf("tls_auth(%p): %p\n", node, node->ctx);
#endif

	if (node->trace)
		fprintf(trace_file, "%p: tls_auth\n", node);

	node->ctx = SSL_new(tls_ctx);

	if (!node->ctx) {

		printf("SSL: Failed to create new context -- disabling\n");
		node->use_ssl = 0;

		return -1;

	}


	if (RAND_status() != 1) {
		printf("ssl_init: System without /dev/urandom, PRNG seeding must be done manually\n");
		node->use_ssl = 0;

		return -2;
	}


	SSL_set_cipher_list(node->ctx, ssl_tlsciphers);
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(node->ctx, SSL_OP_NO_COMPRESSION);
#endif

	SSL_CTX_set_options(node->ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

#ifdef DEBUG
	printf("Setting fd %d\n", node->socket);
#endif

	if (SSL_set_fd(node->ctx, node->socket ) != 1)
		printf("SSL_set_fd_error");


	// Experimental
	node->status = ST_CONNECTED;
	node->auth_type = LION_SSL_SERVER;


#ifdef DEBUG
	printf("Status %d\n", node->status);
#endif

	tls_cont_auth(node);

	return 1;
}



void tls_cont_auth( connection_t *node )
{
	int status, sslerr;
	char cipher_info[128];
	SSL_CIPHER *cipher;


	if (node->trace)
		fprintf(trace_file, "%p: tls_cont_auth\n", node);

#ifdef DEBUG
	printf("tls_cont_auth: ... %p \n", node);
#endif
	//net_want_select( node->handle, NET_WANT_NONE);
	node->want_mode = NET_WANT_NONE;

	status = SSL_accept(node->ctx);
	sslerr = SSL_get_error(node->ctx,status);


	//	printf("Status %d\n", status);


	if (status == 1) {

		cipher = SSL_get_current_cipher(node->ctx);
		SSL_CIPHER_description(cipher, cipher_info, sizeof(cipher_info));

#ifdef DEBUG
		printf("tls_auth_cont: connection %p switched to encryption! Protocol version: %s\n",node, SSL_get_version(node->ctx));

 		printf("tls_auth_cont: cipher %s\n", cipher_info);
#endif


		//printf("[tls] setting misc crap\n");
		//SSL_set_mode(node->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
		//SSL_set_mode(node->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		SSL_set_mode(node->ctx,
					 SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);


		node->use_ssl = 1;
		node->status  = ST_CONNECTED;

		node->auth_type = LION_SSL_OFF;

		// POST EVENT!
		_lion_userinput( node,
						node->user_data,
						LION_CONNECTION_SECURE_ENABLED,
						0, cipher_info);

		// If we are in delay, we also need to issue connected now.
		// that is, if we are still connected
		if (node->want_ssl == LION_SSL_SERVER) {

			node->want_ssl = LION_SSL_OFF;

			if (node->status != ST_DISCONNECT)
				_lion_userinput( node,
								node->user_data,
								LION_CONNECTION_CONNECTED,
								0, NULL);

		}

		return;

	} else {

		//node->ssl_fd_mode = TLS_NONE;

		switch (sslerr) {
		case SSL_ERROR_WANT_READ:
			node->want_mode = NET_NEG_WANT_READ;
#ifdef DEBUG
			printf("tls_auth_cont: Setting wantread - %d\n", node->status);
#endif
			break;
		case SSL_ERROR_WANT_WRITE:
#ifdef DEBUG
			printf("tls_auth_cont: Setting wantwrite - %d\n", node->status);
#endif
			node->want_mode = NET_NEG_WANT_WRITE;
			break;
		default:
#ifdef DEBUG
   			printf("tls_auth_cont: failed (%d, %d, %d)\n",
				   sslerr, status, errno);
#endif

			if (node->trace)
				fprintf(trace_file, "%p: tls_auth_cont: failed (%d, %d, %d) => %s\n",
						node,
						sslerr, status, errno,
						(char *)ERR_error_string(ERR_get_error(), NULL));

			node->use_ssl = 0;
			node->auth_type = LION_SSL_OFF;
			node->want_ssl  = LION_SSL_OFF;
			node->status = ST_CONNECTED;

			// POST EVENT!
			_lion_userinput( node,
							node->user_data,
							LION_CONNECTION_SECURE_FAILED,
							0, NULL); // fixme, SSL_get_error


		}
	}
}









int tls_read(connection_t *node, void *buf, int num)
{
	SSL *ssl;
	int status, sslerr;

	ssl = node->ctx;

	status = SSL_read(ssl, (char *)buf, num);
	sslerr = SSL_get_error(ssl, status);

#ifdef DEBUG_VERBOSE
	printf("tls_read: %p: (SSL_read %d, sslerr %d, errno %d): ctx %p: %s\n",
	       node,status,
	       sslerr, errno,
	       node->ctx,
	       (char *)ERR_error_string(ERR_get_error(), NULL));
#endif
#ifdef DEBUG
	if (node->trace)
		fprintf(trace_file, "%p: lion_read(%d,%d,%d)\n", node,status,
			sslerr, errno);
#endif

	if (status == -1) {

		//ud->ssl_ctrl_func = TLS_READ;

		switch (sslerr) {

			// In READ mode, we get this "error" when we normally want more
			// to read, ie, EWOULDBLOCK. So it isn't really something we set
			// for read. We DO set it for write?
		case SSL_ERROR_WANT_READ:

		  // SSL_read returns WANT_READ even when it is just
		  // EWOULDBLOCK (wait for more input)
#ifdef EWOULDBLOCK
		  if (errno == EWOULDBLOCK) return -1;
#endif
#ifdef EAGAIN
		  if (errno == EAGAIN) return -1;
#endif


#ifdef DEBUG
		  printf("tls_read: Setting wantread - %d -> %d\n",
			 node->want_mode, NET_READ_WANT_READ);
#endif

		  node->want_mode = NET_READ_WANT_READ;
		  return -2;
		  break;

			// Here, it is actually the same as "BUFFERFULL" status.
		case SSL_ERROR_WANT_WRITE:
#ifdef DEBUG
		  printf("tls_read: Setting wantwrite - %d -> %d\n",
			 node->want_mode, NET_READ_WANT_WRITE);
#endif
			node->want_mode = NET_READ_WANT_WRITE;
			return -2;
			break;
		default:
#ifdef DEBUG
		  printf("tls_read_error: %p %d,%d,%d: %s\n", node, sslerr,
			 status, errno,
			 (char *)ERR_error_string(ERR_get_error(), NULL));
#endif
			return -1;
		}

	} else {

	  if ((node->want_mode == NET_READ_WANT_READ) ||
	      (node->want_mode == NET_READ_WANT_READ)) {
	    node->want_mode = NET_WANT_NONE;
	  }

	}

	return status;
}


int tls_write(connection_t *node, void *buf, int num)
{
	SSL *ssl;
	int status, sslerr;

	ssl = node->ctx;

	if (!node->ctx) {
		printf("[tls_write] ->ctx is NULL!?!\n");
		return -1;
	}

#if 0
	int poo;

	poo = fcntl(SSL_get_fd(ssl), F_GETFL);
	if (!(poo & O_NONBLOCK))
		printf("Warning, %d %d (%d)is blocking.\n",
			   SSL_get_fd(ssl), poo, poo & O_NONBLOCK);
#endif




	status = SSL_write(ssl, (char *)buf, num);
	sslerr = SSL_get_error(ssl, status);

#if 0
 {
	 time_t t;
	 time(&t);
	if (t - lion_global_time >= 2) {
		printf("SSL_write: operation took long time!\n");
		poo = fcntl(SSL_get_fd(ssl), F_GETFL);
		printf("Is nonblock: fd %d %d (%d)\n",
			   SSL_get_fd(ssl), poo, poo & O_NONBLOCK);
		sockets_setnonblocking(SSL_get_fd(ssl));
	}
 }
#endif


#ifdef DEBUG_VERBOSE
	printf("tls_write: %d - %s\n", status, (char *)buf);
#endif
#ifdef DEBUG
	if (node->trace)
		fprintf(trace_file, "%p: tls_write(%d,%d,%d)\n", node,status,
			sslerr, errno);
#endif


	if (status == -1) {

		//ud->ssl_ctrl_func = TLS_READ;

		switch (sslerr) {

		case SSL_ERROR_WANT_READ:
#ifdef DEBUG
		  printf("tls_write: Setting wantread - %d -> %d\n",
			 node->want_mode, NET_WRITE_WANT_READ);
#endif
		  node->want_mode = NET_WRITE_WANT_READ;

		  return -2;
		  break;

		case SSL_ERROR_WANT_WRITE:
#ifdef DEBUG
		  printf("tls_write: Setting wantwrite - %d -> %d\n",
			 node->want_mode, NET_WRITE_WANT_WRITE);
#endif
			node->want_mode = NET_WRITE_WANT_WRITE;
			return -2;
			break;
		default:
#ifdef DEBUG
			printf("tls_write_error: %p %d,%d,%d\n", node, sslerr,
				   status, errno);
#endif
			// This is often EAGAIN error, so return it as a real error
			// then we check against EAGAIN.
			return -1;
		}

	} else {

	  if ((node->want_mode == NET_WRITE_WANT_READ) ||
	      (node->want_mode == NET_WRITE_WANT_WRITE)) {
	    node->want_mode = NET_WANT_NONE;
	  }

	}

	return status;
}












int tls_clauth( connection_t *node )
{

#ifdef DEBUG
	printf("tls_clauth(%p): %p\n", node, node->ctx);
#endif

	if (node->trace)
		fprintf(trace_file, "%p: lion_clauth\n", node);

	node->ctx = SSL_new(tls_ctx);

	if (!node->ctx) {

		printf("SSL: Failed to create new context -- disabling: %s\n",
			(char *)ERR_error_string(ERR_get_error(), NULL));
		node->use_ssl = 0;

		return -1;

	}

	if (RAND_status() != 1) {
		printf("ssl_init: System without /dev/urandom, PRNG seeding must be done manually\n");
		node->use_ssl = 0;

		return -2;
	}


	// This breaks on Z
	//SSL_set_cipher_list(node->ctx, ssl_tlsciphers);
#ifdef SSL_OP_NO_COMPRESSION
	//SSL_CTX_set_options(node->ctx, SSL_OP_NO_COMPRESSION);
#endif

#ifdef DEBUG
	printf("Setting fd %d ciphers %s\n", node->socket, ssl_tlsciphers);
#endif

	if (SSL_set_fd(node->ctx, node->socket ) != 1)
		printf("SSL_set_fd_error");


	// Experimental
	node->status = ST_SSL_NEG;
	node->auth_type = LION_SSL_CLIENT;


#ifdef DEBUG
	printf("Status %d\n", node->status);
#endif

	tls_cont_clauth(node);

	return 1;
}



void tls_cont_clauth( connection_t *node )
{
	int status, sslerr;
	char cipher_info[128];
	SSL_CIPHER *cipher;

	if (node->trace)
		fprintf(trace_file, "%p: tls_cont_clauth\n", node);

#ifdef DEBUG
	printf("tls_cont_clauth: ... \n");
#endif
	//net_want_select( node->handle, NET_WANT_NONE);
	node->want_mode = NET_WANT_NONE;

	status = SSL_connect(node->ctx);
	sslerr = SSL_get_error(node->ctx,status);


	//	printf("Status %d\n", status);


	if (status == 1) {

		cipher = SSL_get_current_cipher(node->ctx);
		SSL_CIPHER_description(cipher, cipher_info, sizeof(cipher_info));

#ifdef DEBUG
		printf("tls_clauth_cont: connection %p switched to encryption! Protocol version: %s\n",node, SSL_get_version(node->ctx));

 		printf("tls_clauth_cont: cipher %s\n", cipher_info);
#endif

		node->use_ssl = 1;

		node->status  = ST_CONNECTED;

		node->auth_type = LION_SSL_OFF;

		//SSL_set_mode(node->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		SSL_set_mode(node->ctx,
					 SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);

		// POST EVENT!
		_lion_userinput( node,
						node->user_data,
						LION_CONNECTION_SECURE_ENABLED,
						0, cipher_info);

		// If we are in delay, we also need to issue connected now.
		if (node->want_ssl == LION_SSL_CLIENT) {

			node->want_ssl = LION_SSL_OFF;

			_lion_userinput( node,
							node->user_data,
							LION_CONNECTION_CONNECTED,
							0, NULL);
		}

		return;

	} else {

		//node->ssl_fd_mode = TLS_NONE;

		switch (sslerr) {
		case SSL_ERROR_WANT_READ:
			node->want_mode = NET_NEG_WANT_READ;
#ifdef DEBUG
			printf("Setting wantread - %d\n", node->status);
#endif
			break;
		case SSL_ERROR_WANT_WRITE:
			node->want_mode = NET_NEG_WANT_WRITE;
#ifdef DEBUG
			printf("Setting wantwrite - %d\n", node->status);
#endif
			break;
		default:
#ifdef DEBUG
		  printf("tls_clauth_cont: failed (status,sslerr,errno) %d %d %d: %s\n", status, sslerr, errno,
			 (char *)ERR_error_string(ERR_get_error(), NULL));
#endif
			if (node->trace)
				fprintf(trace_file, "%p: tls_clauth_cont: failed (%d, %d, %d) => %s\n",
						node,
						sslerr, status, errno,
						(char *)ERR_error_string(ERR_get_error(), NULL));

			node->use_ssl = 0;
			node->want_ssl = LION_SSL_OFF;
			node->auth_type = LION_SSL_OFF;

			// POST EVENT!
			_lion_userinput( node,
							node->user_data,
							LION_CONNECTION_SECURE_FAILED,
							0, NULL); // fixme

			// if client haven't disconnected, we send connected_event
			if (node->status != ST_DISCONNECT) {

				// If we are in delay, we also need to issue connected now.
				if (node->want_ssl == LION_SSL_CLIENT) {

					node->status   = ST_CONNECTED;

					_lion_userinput( node,
									node->user_data,
									LION_CONNECTION_CONNECTED,
									0, NULL);

				}
			}
		}
	}
}




//
// return 0 - is SSL auth started - SSL_NEG status is set.
// return 1 - plain text input.
//
int tls_peek(connection_t *node)
{
	char buffer;
	int ret;

	// peek() ahead, check if its SSL handshake.
	// if it is, start authentication, if not, return.

	ret = sockets_peek(node->socket, &buffer, 1);

#ifdef DEBUG
	printf("SSL Autosensing %d:%x\n", ret, buffer);
#endif

	if (ret == 1) {

		// check for SSL handshake, currently, only v2 and v3 is known.
		if ((buffer == SSL3_RT_HANDSHAKE) ||
			(buffer & 0x80)) {

#ifdef DEBUG
			printf("Autosensed SSL handshake\n");
#endif

			return tls_auth(node);

		}

	}

	// It is plain-text, we need to send sec_failed and connected events.

	node->use_ssl   = 0;
	node->want_ssl  = LION_SSL_OFF;
	node->auth_type = LION_SSL_OFF;


	_lion_userinput( node,
					node->user_data,
					LION_CONNECTION_SECURE_FAILED,
					0, NULL); // fixme


	if (node->socket != ST_DISCONNECT) {

		node->status   = ST_CONNECTED;

		_lion_userinput( node,
						node->user_data,
						LION_CONNECTION_CONNECTED,
						0, NULL);
	}


	return 1;

}


//
// return <int> - return number of bytes in SSL buffers
//
int tls_pending(connection_t *node)
{
	if (!node || !node->ctx) return 0;

	return SSL_pending(node->ctx);
}





#endif // WITH_SSL

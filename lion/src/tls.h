
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

// $Id: tls.h,v 1.2 2007/04/14 03:23:46 lundman Exp $
// SSL/TLS Support Functions
// Jorgen Lundman October 17th, 2002.

#ifdef WITH_SSL

#ifndef TLS_H_INCLUDED
#define TLS_H_INCLUDED



enum {TLS_CLEAR = 0, TLS_PRIV};             /* Data modes */
enum {TLS_NONE = 0, TLS_READ, TLS_WRITE};   /* Restart modes */


extern int net_client_SSL;
extern int net_server_SSL;

extern char *ssl_tlsciphers;
extern char *ssl_tlsrsafile;
extern char *ssl_egdsocket;





int     tls_init( void );
void    tls_free( void );
int     tls_auth( connection_t * );
void    tls_cont_auth( connection_t * );
void    tls_close( connection_t * );
int     tls_clauth( connection_t * );
void    tls_cont_clauth( connection_t * );

int     tls_read(connection_t *node, void *buf, int num);
int     tls_write(connection_t *node, void *buf, int num);

int     tls_peek(connection_t *node);
int     tls_pending(connection_t *node);


#endif // TLS_H_INCLUDED
#endif // WITH_SSL

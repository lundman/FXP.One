// $Id: http.c,v 1.19 2012/06/30 06:08:35 lundman Exp $
//
// Jorgen Lundman 18th December 2003.
//
// Command Channel Functions. (GUI's talking to engine)
//
// This takes new connections on the command socket, and once they are
// authenticated, passes the connection to command2.c
//
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "lion.h"
#include "misc.h"
#include "base64.h"

#include "debug.h"
#include "command.h"
#include "command2.h"
#include "settings.h"
#include "parser.h"
#include "engine.h"
#include "users.h"
#include "http.h"
#include "lfnmatch.h"

#define _GNU_SOURCE
#include <string.h>

#include "version.h"

//#define VERBOSE

#ifdef WIN32
#define _WINDOWS_MEAN_AND_LEAN
#include <io.h>
#include "win32.h"
typedef BYTE uint8_t;
typedef DWORD uint32_t;
#define VERSION "2.0-win32"
#define stat _stati64
#define fstat _fstati64
#endif

#define HTTP_USE_SSL


#ifdef WITH_SSL

//#if defined(__APPLE__)
//#  define COMMON_DIGEST_FOR_OPENSSL
//#  include <CommonCrypto/CommonDigest.h>
//#  define SHA1 CC_SHA1
//#else
//#  include <openssl/sha.h>
//#endif

#include <openssl/sha.h>
#endif

//
// Open the listening port, and await connections.
// Upon connection (check SSL only) have client authenticate.
// Then pass onto command2.
//

static lion_t *http_listen = NULL;



// This is an internal value to LION, you can check if the certificate was
// read such that we can be in server mode.
extern int net_server_SSL;

int http_handler(lion_t *handle, void *user_data,
                 int status, int size, char *line);


void http_init(void)
{
#ifdef WITH_SSL

	if (http_listen)
		http_free();

	http_listen = lion_listen(&settings_values.http_port,
                              settings_values.http_iface,
                              LION_FLAG_EXCLUSIVE,
                              NULL);

	if (!http_listen) {

		perror("[http]");
		debugf("[http] listening socket failed to open: \n");

		engine_running = 0;

		return;
	}


	//  fulfill means we can just assume it will not be NULL
	lion_set_handler(http_listen, http_handler);

	debugf("[http] listening on port %u\n",
		   settings_values.http_port);

    if (engine_firstrun) {
        printf("\nWebSockets client running. Use a browser capable of key1/key2 websockets\n"
               "(currently, only Chrome) To login to FXP.One, create sites, FXP and more.\n");
        printf("Use the following URL to connect to WFXP:\n");
        printf("machine, you can use 127.0.0.1 as the IP.\n");
        printf("https://name.or.ip.of.this.pc:%u/\n\n",
               settings_values.http_port);
        printf("If browser and engine are on the same machine, you can use:\n"
               "https://127.0.0.1:%u/\n\n",
               settings_values.http_port);
        printf("In WFXP login window, you might need to update the wss://localhost:8886/ as well.\n");
    }
#endif
}


void http_free(void)
{
#ifdef WITH_SSL
	if (!http_listen) return;

	lion_close(http_listen);

	http_listen = NULL;
#endif
}



#ifdef WITH_SSL

char *hide_path(char *s)
{
    char *r;
    r = strrchr(s, '/');
    return ((r && r[1]) ? &r[1] : s);
}

void http_parser_free(http_parser_t *parser)
{
    SAFE_FREE(parser->path);
    SAFE_FREE(parser->request);
    SAFE_FREE(parser->host);
    SAFE_FREE(parser->connection);
    SAFE_FREE(parser->key);
    SAFE_FREE(parser->origin);
    SAFE_FREE(parser->protocol);
    SAFE_FREE(parser->version);
    SAFE_FREE(parser);
}

char *http_get_mime(http_parser_t *parser)
{
    if (!parser || !parser->path) return "text/html";
    if (!lfnmatch("*.js", parser->path, LFNM_CASEFOLD))
        return "application/javascript";
    if (!lfnmatch("*.html", parser->path, LFNM_CASEFOLD))
        return "text/html";
    if (!lfnmatch("*.css", parser->path, LFNM_CASEFOLD))
        return "text/css";
    if (!lfnmatch("*.png", parser->path, LFNM_CASEFOLD))
        return "image/png";
    if (!lfnmatch("*.jpg", parser->path, LFNM_CASEFOLD))
        return "image/jpeg";
    if (!lfnmatch("*.gif", parser->path, LFNM_CASEFOLD))
        return "image/gif";
    return "text/html";
}


char *mystrcat(char *a, char *b)
{
    int len_a,len_b;
    char *r;

    if (!a || !b) return NULL;

    len_a = strlen(a);
    len_b = strlen(b);

    if (!len_a && !len_b) return strdup("");
    if (!len_a) return strdup(b);
    if (!len_b) return strdup(a);

    r = malloc(len_a + len_b + 1);
    if (!r) return NULL;

    strcpy(r, a);
    strcat(r, b);

    return r;
}






//
// The framed handler gets data from the client, in framed format
// INPUT: http_handle (framed)
// OUTPUT: engine_handle (TEXT)
//
int http_framed_handler(lion_t *handle, void *user_data,
                        int status, int size, char *line)
{
	unsigned long host;
	int port;
    http_relay_t *relay;

    relay = (http_relay_t *) user_data;

	switch(status) {

	case LION_CONNECTION_LOST:

		/* FALL THROUGH */
	case LION_CONNECTION_CLOSED:

		lion_getpeername(handle, &host, &port);

		if (engine_running)
			debugf("[http] WebSocket connection %s %s:%u %d:%s\n",
				   "closed",
				   lion_ntoa(host), port,
				   size, line ? line : "(null)");

        lion_set_userdata(handle, NULL);
        if (relay) {
            relay->http_handle = NULL;
            if (relay->engine_handle) {
                lion_set_userdata(relay->engine_handle, NULL);
                lion_disconnect(relay->engine_handle);
            }
        }
        SAFE_FREE(relay);
        relay = NULL;
		break;


	case LION_CONNECTION_SECURE_ENABLED:
		debugf("[http] connection upgraded to secure\n");
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("[http] connection failed to SSL\n");
		break;

    case LION_BINARY:
        if (size >= 4) {
            lion64u_t len;
            uint32_t len1;
            uint32_t in, fin, opcode, mask;
            unsigned char key[4];
            int i;

            debugf("[http] framed input %d\n", size);
#ifdef VERBOSE
            debugf("first 32bit\n");
            for (i=0; i<8; i++)
                debugf("%02X ", (unsigned char)line[i]);
            debugf("\n");
#endif
            in = 0;
            fin    = (unsigned char)line[in] & 0x80;
            opcode = (unsigned char)line[in] & 0x0f;
            in++;
            mask   = (unsigned char)line[in] & 0x80;
            len1   = (unsigned char)line[in] & 0x7f;
            in++;

            debugf("[http] FIN:%d opcode %X MASK:%d len %d\n",
                   fin?1:0, opcode, mask?1:0, len1);

            // Client MUST set MASK
            if (!mask) {
                debugf("[http] client-frame without MASK set. dropping\n");
                lion_disconnect(handle);
                return 0;
            }

            // Close connection?
            if (opcode == 0x8) {
                debugf("[http] client-frame with CLOSE. Goodbye\n");
                lion_close(handle);
                return 0;
            }

            // Reversed, unknown opcodes
            if ((opcode == 0x3) ||
                (opcode == 0x4) ||
                (opcode == 0x5) ||
                (opcode == 0x6) ||
                (opcode == 0x7) ||
                (opcode == 0xb) ||
                (opcode == 0xc) ||
                (opcode == 0xd) ||
                (opcode == 0xe) ||
                (opcode == 0xf)) {
                debugf("[http] client-frame with unknown opcode %x\n", opcode);
                lion_disconnect(handle);
                return 0;
            }

            len = 0;
            // Do we need to read extended lengths?
            if (len1 == 126) { // 16 bit length
                len = (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
            } else if (len1 == 127) { // 63 bit length
                len = (unsigned char)(line[in++]&0x7f); // top bit must be 0
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
                len |= (unsigned char)line[in++];
                len <<= 8;
            } else {
                len = len1;
            }
            debugf("[http] client-frame len of %"PRIu64" bytes (in %d)\n",
                   len, in);

            // Read in mask-key
            key[0] = (unsigned char)line[in++];
            key[1] = (unsigned char)line[in++];
            key[2] = (unsigned char)line[in++];
            key[3] = (unsigned char)line[in++];

            debugf("[http] client-frame ZE KEY IS %02X%02X%02X%02X\n",
                   key[0], key[1], key[2], key[3]);


            // Ok, now "line[in]" is start of payload, and hopefully
            // we have all of it in this buffer.
            //debugf("[http] size is %d and in now at %d\n", size, in);

            size -= in;

            if ((lion64u_t)size < len) {
                debugf("[http] Here's the thing, we didn't get the whole payload data in one buffer, and lundman has not written the code to handle it split up, so this code will now just drop the packet. You should bug lundman to fix this.\n");
                break;
            }

            // Decode buffer!
            for (i = 0; i < len; i++) {
                line[in + i] ^= key[i%4];
            }

            // It might not be terminated
            line[in + len] = 0;

            if (relay && relay->engine_handle) {

                if (strchr(&line[in], '\n')) {
                    lion_printf(relay->engine_handle,
                                "%s",
                                &line[in]);
                    //lion_output(relay->engine_handle,
                    //           &line[in],
                    //           in + len);
                } else {

                    lion_printf(relay->engine_handle,
                                "%s\r\n",
                                &line[in]);

                }

                debugf("[http] finally passing to engine: '%s'\n",
                       &line[in]);
            }
        }
        break;


    }

    return 0;
}



//
// engine_handler talks to the engine, andrelays to client
// INPUT: engine_handle (TEXT)
// OUTPUT: http_handle (framed)
//
int http_engine_handler(lion_t *handle, void *user_data,
                        int status, int size, char *line)
{
	unsigned long host;
	int port;
    http_relay_t *relay;

    relay = (http_relay_t *) user_data;

	switch(status) {

	case LION_CONNECTION_LOST:

		/* FALL THROUGH */
	case LION_CONNECTION_CLOSED:

		lion_getpeername(handle, &host, &port);

		if (engine_running)
			debugf("[http] engine connection %s %s:%u %d:%s\n",
				   "closed",
				   lion_ntoa(host), port,
				   size, line ? line : "(null)");

        if (relay) {
            relay->engine_handle = NULL;
            if (relay->http_handle)
                lion_disconnect(relay->http_handle);
        }
        relay = NULL;
		break;

    case LION_CONNECTION_CONNECTED:

        // If the HTTP connection is actually SSLed, HTTPS, we will
        // update the localhost connection to SSL as well. Seems a little
        // overkill, but it lets us have the engine know of the SSL
        // status easily.
        if (relay && relay->http_handle) {
            if (lion_ssl_enabled(relay->http_handle)) {
                debugf("[http] upgrading engine connection to SSL\n");
                relay->ssl_negotiating = 1;
                lion_printf(handle, "SSL\r\n");
                return 0;
            }
        }

        if (relay && relay->http_handle)
            lion_enable_read(relay->http_handle);
        break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf("[http] connection upgraded to secure\n");
        if (relay) {
            relay->ssl_negotiating = 0;
            if (relay->http_handle)
                lion_enable_read(relay->http_handle);
        }
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("[http] connection failed to SSL\n");
        lion_disconnect(handle);
		break;

    case LION_INPUT:
        if (relay && relay->ssl_negotiating) {
            if (line && !strncasecmp("SSL|CODE=0|", line, 11)) {
                // we assume any input is the start of SSL negotiations,
                // even if it is an error, the SSL will then fail, and
                // that failure handles the error
                lion_ssl_set(handle, LION_SSL_CLIENT);
                return 0;
            }

        }

        if (relay && relay->http_handle && (size>0) && line && *line) {
            char *send = NULL;
            uint32_t len = 0;
            debugf("[http] engine '%s'\n", line);
            // Make 'framed' buffer, and send it.

            /*
              Payload length:  7 bits, 7+16 bits, or 7+64 bits

              The length of the "Payload data", in bytes: if 0-125, that is the
              payload length.  If 126, the following 2 bytes interpreted as a
              16-bit unsigned integer are the payload length.  If 127, the
              following 8 bytes interpreted as a 64-bit unsigned integer (the
              most significant bit MUST be 0) are the payload length.  Multibyte
              length quantities are expressed in network byte order.  Note that
              in all cases, the minimal number of bytes MUST be used to encode
              the length, for example, the length of a 124-byte-long string
              can't be encoded as the sequence 126, 0, 124.  The payload length
              is the length of the "Extension data" + the length of the
              "Application data".  The length of the "Extension data" may be
              zero, in which case the payload length is the length of the
              "Application data".
            */

            if (size < 124) { // 126 - 2 headersize.

                send = mystrcat("ABZZ", line); // 16bits + \r\n
                size += 2 ; // because of \r\n
                send[0] = 0x81; // FIN + text
                send[1] = size; // MASK 0, and 7 bits of size (less than 126, with \r\n included)
                strcpy(&send[2], line); // payload
                strcpy(&send[size], "\r\n");
                len = size + 2; // total send size,adding 16bits framed

            } else if ((size > 123) && // -2 bytes
                       (size < 65534)) { // 16 bit size

                send = mystrcat("ABCCZZ", line); // 32bits + \r\n
                send[0] = 0x81; // FIN + text
                send[1] = 126; // 16bit length used
                size += 2; // + \r\n
                send[2] = (size&0xff00) >> 8;
                send[3] = (size&0xff);

                strcpy(&send[4], line); // payload
                strcpy(&send[size+2], "\r\n");
                len = size + 4; // total send size, adding 32bits framed

                //debugf("%d SEND %08X %08X %08X %08X\n",
                //      size, send[0], send[1], send[2], send[3]);

            } else {
                debugf("[http] Unhandled size!\n");
            }

            if (send && len) {
                debugf("[http] sending %d bytes to client\n", len);
                len = lion_output(relay->http_handle,
                                  send, len);
#ifdef VERBOSE
                int i;
                for (i =0; i < len; i++)
                    debugf("%02X ", (unsigned char)send[i]);
                debugf("\n");
#endif
            }

            SAFE_FREE(send);
        }
        break;

    }
    return 0;
}





//
// Handler for GET requests. Open file, send contents.
// INPUT: parser->file_handle
// OUTPUT: http_handle
// userdata: parser
//
int http_get_handler(lion_t *handle, void *user_data,
                     int status, int size, char *line)
{
	unsigned long host;
	int port;
    http_parser_t *parser;
    lion64u_t len = 0;
    struct stat stbf;

    parser = (http_parser_t *) lion_get_userdata(handle);

	switch(status) {

	case LION_CONNECTION_NEW:
		break;

	case LION_CONNECTION_CONNECTED:
		break;

    case LION_FILE_OPEN:
        if (parser && parser->http_handle)

            if (!fstat(lion_fileno(handle), &stbf))
                len = (lion64u_t) stbf.st_size;
            lion_printf(parser->http_handle,
                        "HTTP/1.0 200 OK\r\n"
                        "SERVER: FXP.One (%s)\r\n"
                        "Content-Length: %"PRIu64"\r\n"
                        "Content-Type: %s\r\n"
                        "\r\n",
                        VERSION,
                        len,
                        http_get_mime(parser));
        break;

    case LION_FILE_CLOSED:
        if (parser) {

            debugf("[http] file closed.\n");

            parser->file_handle = NULL;
            lion_set_userdata(handle, NULL);

            if (parser->http_handle) {
                if (parser->keep_alive) {
                    lion_set_handler(parser->http_handle, http_handler);
                    lion_enable_read(parser->http_handle);
                    lion_set_userdata(parser->http_handle, NULL);
                    http_parser_free(parser);
                    return 0;
                } else {
                    lion_close(parser->http_handle);
                }
            } // http
        } // parser
        break;

	case LION_CONNECTION_LOST:

		/* FALL THROUGH */
	case LION_CONNECTION_CLOSED:

        if (parser) {

            if (parser->file_handle) {
                lion_set_userdata(parser->file_handle, NULL);
                lion_disconnect(parser->file_handle);
                parser->file_handle = NULL;
            }

            lion_set_userdata(handle, NULL);
            http_parser_free(parser);
        }

        lion_getpeername(handle, &host, &port);

        if (engine_running)
            debugf("[http] GET connection %s %s:%u %d:%s\n",
                   "closed",
                   lion_ntoa(host), port,
                   size, line ? line : "(null)");

		break;

	case LION_BINARY:
        if (line && parser && parser->http_handle && size > 0) {
            lion_output(parser->http_handle, line, size);
            }
		break;
	}

	return 0;

}





//
// The main handler for the listening command channel, as well as
// "anonymous" connections. Once they have authenticated, we move them
// to a new handler.
//
int http_handler(lion_t *handle, void *user_data,
					int status, int size, char *line)
{
	unsigned long host;
	int port;
    http_parser_t *parser;
    char *ar, *token;

    parser = (http_parser_t *) lion_get_userdata(handle);

	switch(status) {

	case LION_CONNECTION_NEW:
		lion_set_handler(
						 lion_accept(
									 handle, 0, LION_FLAG_FULFILL,
									 NULL, NULL, NULL),
						 http_handler);
		break;

	case LION_CONNECTION_CONNECTED:

		lion_getpeername(handle, &host, &port);
		debugf("[http] New connection from %s:%u\n",
			   lion_ntoa(host), port);
#ifdef HTTP_USE_SSL
        lion_ssl_set(handle, LION_SSL_SERVER);
#endif
		break;


	case LION_CONNECTION_LOST:

		/* FALL THROUGH */
	case LION_CONNECTION_CLOSED:

        if (parser)
            http_parser_free(parser);
        parser = NULL;

		lion_getpeername(handle, &host, &port);

		if (engine_running)
			debugf("[http] connection %s %s:%u %d:%s\n",
				   "closed",
				   lion_ntoa(host), port,
				   size, line ? line : "(null)");

		break;


	case LION_CONNECTION_SECURE_ENABLED:
		debugf("[http] connection upgraded to secure\n");
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("[http] connection failed to SSL\n");
        lion_disconnect(handle);
		break;


	case LION_INPUT:
		debugf("[http] input '%s'\n", line);

        if (!parser && !strncmp("GET ", line, 4)) {

            parser = (http_parser_t *) calloc(sizeof(*parser), 1);
            if (!parser) {
                lion_disconnect(handle);
                return 0;
            }
            lion_set_userdata(handle, (void *)parser);

            debugf("[http] parsing request\n");
            parser->request = strdup(line);

            ar = &line[4]; // Past GET
            token = misc_digtoken(&ar, " \r\n");
            if (token) {
                if (!mystrccmp("/", token))
                    parser->path = strdup(HTTP_DEFAULT_FILE);
                else
                    parser->path = strdup(token);
            }
            token = misc_digtoken(&ar, " \r\n");
            if (token) parser->version = strdup(token);

            break;
        }

        if (!parser && !strncmp("QUIT", line, 4)) {
            lion_disconnect(handle);
            return 0;
        }

        if (parser) {

            // Header finished
            if (*line == 0) {

                // Check for WebSocket!
                if (parser->upgrade && parser->connection &&
                    parser->key && parser->version) {

                    debugf("[http] header finished, action (%s, %s, %s, %s)\n",
                           parser->upgrade, parser->connection, parser->key,
                           parser->version);

                    if (!strcasecmp("websocket", parser->upgrade) &&
                        strcasestr(parser->connection, "Upgrade") &&
                        atoi(parser->version) >= 8) {
                        char *pre;

                        debugf("[http] valid WebSocket, passing to commands\n");

                        pre = mystrcat(parser->key,
                                       HTTP_GUID);
                        debugf("cat is '%s'\n", pre);
                        if (pre) {
                            // Perform SHA-1
                            SHA_CTX sha;
                            // Base64 goes +4 over, sigh.
                            uint8_t hash[SHA_DIGEST_LENGTH+4];
                            uint8_t reply[((SHA_DIGEST_LENGTH + 2) * 4 / 3 + 2)];
                            http_relay_t *relay;
                            memset(hash, 0, sizeof(hash));
                            SHA1_Init( &sha );
                            SHA1_Update( &sha, pre, strlen(pre) );
                            SHA1_Final( hash, &sha );
                            SAFE_FREE(pre);

                            // Base64 it
                            base64_encode(hash,
                                          SHA_DIGEST_LENGTH,
                                          reply);

                            debugf("base64 %s :%lu\n", reply,
                                   sizeof(reply));

                            lion_printf(handle,
                                        "HTTP/1.1 101 Switching Protocols\r\n"
                                        "Upgrade: websocket\r\n"
                                        "Connection: Upgrade\r\n"
                                        "Sec-WebSocket-Accept: %s\r\n"
                                        "\r\n",
                                        reply);

                            lion_set_handler(handle, http_framed_handler);
                            lion_enable_binary(handle);
                            lion_disable_read(handle);
                            // Now we connect to the normal engine
                            // port and start relaying packets between
                            // the two. Once the connection is established
                            // read is enabled again.
                            relay = (http_relay_t *) calloc(sizeof(*relay),1);
                            if (!relay) {
                                lion_disconnect(handle);
                                return 0;
                            }
                            relay->http_handle = handle;
                            lion_set_userdata(handle, (void *)relay);
                            relay->engine_handle =
                                lion_connect(
                                             settings_values.command_iface ?
                                             lion_ntoa(settings_values.command_iface) :
                                             "127.0.0.1",
                                             settings_values.command_port,
                                             0,
                                             0,
                                             LION_FLAG_FULFILL,
                                             relay);
                            lion_set_handler(relay->engine_handle,
                                             http_engine_handler);
                            http_parser_free(parser);
                            return 0;
                        }

                    } // header values are good

                } else { // WebSockets, or GET

                    debugf("[http] Attempting regular GET\n");

                    // Let's change it to a regular GET
                    if (parser->path) {
                        char *path, *r;

                        if (parser->version &&
                            !mystrccmp("HTTP/1.1", parser->version))
                            parser->keep_alive = 1;

                        if (parser->connection) {
                            if (!mystrccmp("keep-alive", parser->connection))
                                parser->keep_alive = 1;
                            if (!mystrccmp("close", parser->connection))
                                parser->keep_alive = 0;
                        }

                        // We chdir() to fxpone HOME so just open the filename
                        // no path.
                        path = misc_strjoin(HTTP_DIR, parser->path);

                        // For security reasons, we will look for any ".." and
                        // change them to "__". This simplifies security
                        // considerably. And our HTMLs should not use "../"
                        while ((r = strstr(path, ".."))) {
                            r[0] = '_';
                            r[1] = '_';
                        }

                        // HTML will use filename.ext?cgi as well, so terminate at
                        // "?"
                        if ((r = strchr(path, '?'))) {
                            *r = 0;
                        }

                        debugf("[http] GET '%s'\n", path);
                        parser->file_handle = lion_open(path,
                                                        O_RDONLY,
                                                        0400,
                                                        LION_FLAG_NONE,
                                                        NULL);
                        SAFE_FREE(path);
                        if (!parser->file_handle) {
                            debugf("[http] replying with 404 not found.\n");
                            lion_printf(handle, "HTTP/1.0 404 File not found mate\r\n");
                            lion_printf(handle, "SERVER: FXP.Oned (%s)\r\n", VERSION);
                            lion_close(handle);
                            return 0;
                        } // file_handle

                        parser->http_handle = handle;
                        lion_enable_binary(parser->file_handle);
                        lion_set_userdata(parser->file_handle, (void *)parser);
                        lion_set_handler(parser->file_handle, http_get_handler);
                        lion_set_handler(handle, http_get_handler);
                        lion_disable_read(handle);
                        // We don't free 'parser' for GETs.
                        return 0;
                    } // has path

                    // Issue error
                    lion_printf(handle, "HTTP/1.0 500 Error of some kind\r\n");
                    lion_printf(handle, "SERVER: FXP.Oned (%s)\r\n", VERSION);
                    lion_close(handle);
                    return 0;

                } // else not websocket.

                // Since we received a blank line, we release all header
                // values and start again
                http_parser_free(parser);
                lion_set_userdata(handle, NULL);
                return 0;

            } // Blank line, end of headers

            ar = line;
            token = misc_digtoken(&ar, ": \r\n");

            // WebSocket
            if (token && !strcasecmp("Upgrade", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->upgrade = strdup(token);

            } else if (token && !strcasecmp("Connection", token)) {

                //token = misc_digtoken(&ar, " \r\n");
                //if (token) parser->connection = strdup(token);
                parser->connection = strdup(ar);

            } else if (token && !strcasecmp("Host", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->host = strdup(token);

            } else if (token && !strcasecmp("Sec-WebSocket-Key", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->key = strdup(token);

            } else if (token && !strcasecmp("Sec-WebSocket-Origin", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->origin = strdup(token);

            } else if (token && !strcasecmp("Sec-WebSocket-Protocol", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->protocol = strdup(token);

            } else if (token && !strcasecmp("Sec-WebSocket-Version", token)) {

                token = misc_digtoken(&ar, " \r\n");
                if (token) parser->version = strdup(token);

            }

        } // parser

		break;
	}

	return 0;

}


#endif



int http_copyfile(char *src, char *dst)
{
    char buffer[1024];
    int in, out, red;

    in = open(src, O_RDONLY
#ifdef WIN32
              |O_BINARY
#endif
              );
    if (in < 0) {
        perror("open(read): ");
        return 1;
    }

    out = open(dst, O_WRONLY|O_CREAT|O_TRUNC
#ifdef WIN32
              |O_BINARY
#endif
               , 0644);
    if (out < 0) {
        perror("open(write): ");
        close(in);
        return 2;
    }

    while((red = read(in, buffer, sizeof(buffer))) > 0)
        write(out, buffer, red); // error checking?
    close(in);
    close(out);
    return 0;
}

int http_install(char *src, char *dst)
{
    DIR *dirh;
    struct dirent *dirp;
    struct stat stsb;
    char *tmp;
    if (!chdir(src)) {
        dirh = opendir(".");
        if (dirh) {
            while((dirp = readdir(dirh))) {
                if (!strcmp(dirp->d_name, ".")) continue;
                if (!strcmp(dirp->d_name, "..")) continue;

                if (!stat(dirp->d_name, &stsb)) {
                    tmp = misc_strjoin(dst, dirp->d_name);
                    if (S_ISDIR(stsb.st_mode)) {
                        //printf("handling dir '%s': mkdir '%s'\n", dirp->d_name, tmp);
                        mkdir(tmp, 0755);
                        http_install(dirp->d_name, tmp);
                    } else if (S_ISREG(stsb.st_mode)) {
                        //printf("copying file '%s' -> '%s'\n", dirp->d_name, tmp);
                        http_copyfile(dirp->d_name, tmp);
                    }

                    SAFE_FREE(tmp);
                } // Can stat
            } // for all dirents
            closedir(dirh);
        } // can opendir
    } // chdir
    chdir("..");
    return 1;
}



// src: /usr/local/share/wfxp/
// dst: /Users/lundman/.FXP.One/wfxp/
void http_install_wfxp(char *src, char *dst)
{
    // For all files, and directories, copy.
    printf("Installing WFXP... \n");

    mkdir(dst, 0755);
    http_install(src, dst);

    printf("Finished.\n");
}



#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED


// Defines..

#define HTTP_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define HTTP_DIR "./wfxp"
#define HTTP_DEFAULT_FILE "wfxp.html"

// Variables..


struct http_relay_struct {
    lion_t *http_handle;
    lion_t *engine_handle;
    int ssl_negotiating;
};

typedef struct http_relay_struct http_relay_t;

struct http_parser_struct {
    char *path;
    char *request;
    char *host;
    char *upgrade;
    char *connection;
    char *key;
    char *origin;
    char *protocol;
    char *version;
    int keep_alive;
    lion_t *http_handle;
    lion_t *file_handle;
};

typedef struct http_parser_struct http_parser_t;




// Functions..

void      http_init              ( void );
void      http_free              ( void );
int       http_handler           ( lion_t *, void *,	int, int, char * );
void      http_install_wfxp      ( char *src, char *dst );

#endif

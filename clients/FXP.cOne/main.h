#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED


#define VISIBLE_CONNECT  1
#define VISIBLE_SITEMGR  2
#define VISIBLE_EDITSITE 4
#define VISIBLE_DISPLAY  8
#define VISIBLE_QUEUEMGR 16
#define VISIBLE_QLIST    32
#define VISIBLE_SITE     64


extern FILE *d;

extern CDKSCREEN   *cdkscreen;
extern unsigned int visible;




void botbar_print(char *line);
int  botbar_printf(char const *fmt, ...);
void visible_draw(void);

int check_all_mouse(MEVENT *);
void main_connected(void);
void main_site_connected(int);
void main_go(void);



#endif

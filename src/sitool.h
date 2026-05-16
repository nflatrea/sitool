#ifndef SITOOL_H
#define SITOOL_H

#include <lua.h>

#define SITOOL_VERSION "v2.5.052026"

#define SITOOL_MTU       512                    /* max payload bytes (TX/RX)  */
#define SITOOL_DISP      (SITOOL_MTU*3 + 1)     /* hex display buffer         */
#define SITOOL_LINE      1024                   /* max input line / argstr    */
#define SITOOL_ARGV_MAX  64                     /* max tokens per command     */

typedef struct sitool sitool_t;

typedef int (*cmd_t)(sitool_t *st, int argc, char **argv);

typedef struct
{
    const char *name;
    const char *help;
    cmd_t cmd;

} cmd_entry_t;

struct sitool
{
    char prompt[256];
    int  fd;

    /* connection settings */
    char port[128];
    int  baudrate;
    int  databits;
    char parity;
    int  stopbits;
    int  echo;      /* local echo in term mode: 0=off, 1=on   */
    int  dtr;       /* DTR signal: -1=unchanged, 0=low, 1=high */
    int  rts;       /* RTS signal: -1=unchanged, 0=low, 1=high */

    /* lua handler */
    lua_State *L;
    char handler[128];

};

void sitool_init(sitool_t *st);
void sitool_repl(sitool_t *st);
int  sitool_eval(sitool_t *st, char *line);

#endif /* SITOOL_H */

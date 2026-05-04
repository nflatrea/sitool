#ifndef SITOOL_H
#define SITOOL_H

#include <lua.h>

#define SITOOL_VERSION "v2.0.052026"

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

    /* lua handler */
    lua_State *L;
    char       handler[128];
};

void sitool_init(sitool_t *st);
void sitool_repl(sitool_t *st);
int  sitool_eval(sitool_t *st, char *line);

#endif // SITOOL_H

#ifndef SITOOL_H
#define SITOOL_H

#include <lua.h>

#define SITOOL_VERSION "v2.2.052026"

#define AUTO_MAX_RULES 16
#define AUTO_MAX_LEN   256

typedef struct {
    unsigned char pattern[AUTO_MAX_LEN];
    int           plen;
    unsigned char response[AUTO_MAX_LEN];
    int           rlen;
} auto_rule_t;

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
    int  echo; 		// local echo in term mode: 0=off, 1=on
    int  dtr;		// DTR signal: -1=unchanged, 0=low, 1=high
    int  rts;		// RTS signal: -1=unchanged, 0=low, 1=high

    /* lua handler */
    lua_State *L;
    char       handler[128];

    /* auto-answer rules */
    auto_rule_t auto_rules[AUTO_MAX_RULES];
    int         auto_nrules;
};

void sitool_init(sitool_t *st);
void sitool_repl(sitool_t *st);
int  sitool_eval(sitool_t *st, char *line);

#endif // SITOOL_H

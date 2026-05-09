#ifndef MODE_H
#define MODE_H

#include "sitool.h"

typedef struct
{
    int  timeout_ms;   /* poll timeout: -1 = block, >0 = periodic  */
    void (*on_rx)(sitool_t *st, const unsigned char *buf, int len);
    void (*on_key)(sitool_t *st, unsigned char c);  /* NULL = ignore */
    void (*on_tick)(sitool_t *st);                   /* NULL = none   */
} mode_opts_t;

/*
  Enter raw-terminal poll loop on st->fd.
  Catches Ctrl-] to quit. Saves/restores termios.
  Returns 0 normally, -1 on setup error.
*/
int mode_run(sitool_t *st, const mode_opts_t *opts);

#endif /* MODE_H */

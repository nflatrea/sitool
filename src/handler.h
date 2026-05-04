#ifndef HANDLER_H
#define HANDLER_H

#include <stddef.h>

#define HANDLER_PATH_MAX 512
#define HANDLER_MAX      64

typedef struct sitool sitool_t; // def in sitool.h

typedef struct
{
    char name[64];
    char path[HANDLER_PATH_MAX];

} handler_info_t;

int  handler_resolve(const char *name, char *out,size_t max);
int  handler_list(handler_info_t *list, int max);
void handler_list_print(void);
int  handler_load(sitool_t *st, const char *name);
void handler_unload(sitool_t *st);
int  handler_dispatch(sitool_t *st, const char *cmd,
	 int argc, char **argv);
void handler_help(sitool_t *st);
int  handler_send_raw(sitool_t *st, const unsigned char 
	 *data, int len, unsigned char *resp, int resp_max);

/*

handler_resolve

  Resolve a handler name to its full .lua path.
  Search order:
    1. ./handlers/<name>.lua
    2. ~/.local/share/sitool/handlers/<name>.lua
  Returns 0 and fills `out` on success, -1 if not found.

handler_list

  List all available handlers across search paths.
  Fills `list` with up to `max` entries (deduped by name).
  Returns the number of handlers found.

handler_list_print

  Print a formatted list of available handlers to stdout.

handler_load

  Load a Lua handler by name.
  Resolves path, creates a new Lua state, registers the sitool API,
  executes the script, and calls on_load.
  Returns 0 on success, -1 on error.

handler_unload

  Unload the current Lua handler (calls on_unload, closes Lua state).

handler_dispatch

  Try dispatching a command to the loaded Lua handler.
  Returns 1 if handled, 0 if no matching command in handler.

handler_help

  List Lua handler command help.

handler_send_raw

  Send raw bytes and receive response (used by Lua bindings).

*/

#endif // HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "handler.h"
#include "sitool.h"
#include "serial.h"
#include "utils.h"

static int get_search_paths(char paths[][HANDLER_PATH_MAX], int max)
{   
	if (max<=0) return 0;
	
	int n = 0;
    strncpy(paths[n++], "./handlers", HANDLER_PATH_MAX);
	if (n>= max) return n;
	
    const char *home = getenv("HOME");
	if (home == NULL) return n;
	
	snprintf(paths[n++], HANDLER_PATH_MAX,
		"%s/.local/share/sitool/handlers", home);
		
    return n;
}

int handler_resolve(const char *name, char *out, size_t max)
{
    char dirs[2][HANDLER_PATH_MAX];
    int ndirs = get_search_paths(dirs, 2);

    for (int i = 0; i < ndirs; i++) {
        char path[HANDLER_PATH_MAX + 64];
        snprintf(path, sizeof path, "%s/%s.lua", dirs[i], name);

        if (access(path, R_OK) == 0) {
            strncpy(out, path, max);
            out[max - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static int scan_dir(const char *dirpath, handler_info_t *list,
                    int count, int max)
{
    DIR *d = opendir(dirpath);
    if (!d) return count;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        const char *fname = ent->d_name;
        size_t len = strlen(fname);


		// take the filename without extention
        if (len < 5 || strcmp(fname + len - 4, ".lua") != 0)
            continue;

        char name[64];
        size_t nlen = len - 4;
        if (nlen >= sizeof name) nlen = sizeof name - 1;
        memcpy(name, fname, nlen);
        name[nlen] = '\0';

        int dup = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(list[i].name, name) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        strncpy(list[count].name, name, sizeof list[count].name);
        list[count].name[sizeof list[count].name - 1] = '\0';
        snprintf(list[count].path, sizeof list[count].path,
                 "%s/%s", dirpath, fname);
        count++;
    }
    closedir(d);
    return count;
}

int handler_list(handler_info_t *list, int max)
{
    char dirs[2][HANDLER_PATH_MAX];
    int ndirs = get_search_paths(dirs, 2);
    int count = 0;

    for (int i = 0; i < ndirs; i++)
        count = scan_dir(dirs[i], list, count, max);

    return count;
}

void handler_list_print(void)
{
    handler_info_t list[HANDLER_MAX];
    int n = handler_list(list, HANDLER_MAX);

    if (n == 0) {
        printf("  (no handlers found)\n");
        printf("  search paths:\n");
        char dirs[2][HANDLER_PATH_MAX];
        int ndirs = get_search_paths(dirs, 2);
        for (int i = 0; i < ndirs; i++)
            printf("    %s/\n", dirs[i]);
        return;
    }

    for (int i = 0; i < n; i++)
        printf("  %-16s %s\n", list[i].name, list[i].path);
}

/*

	Lua Bindings

	sitool.send(payload_string) 		Send payload (hex, ASCII, or mixed)
	sitool.utils.btoh(raw_string)		Bytes to Hex
	sitool.utils.htob(hex_string)		Hex to Bytes
	sitool.utils.atob(ascii_string)		ASCII to Bytes
	sitool.utils.btoa(raw_string)		Bytes to printable ASCII
	sitool.utils.hex(raw_string)		Hexdump

    H.callbacks.<name>() 				Call <name> if it exists

*/

static const char *ST_KEY = "sitool.context";

static sitool_t *get_st(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, ST_KEY);
    sitool_t *st = (sitool_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return st;
}

/* sitool.send(payload_string), await response
   payload can be hex ("AA BB CC") or mixed hex+ASCII ("02 10 \"HELLO\"") */
static int l_send(lua_State *L)
{
    sitool_t *st = get_st(L);
    const char *payload = luaL_checkstring(L, 1);

    if (st->fd < 0) {
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }

    unsigned char txbuf[256];
    int txlen = parse_payload(payload, txbuf, sizeof txbuf);
    if (txlen <= 0) {
        luaL_error(L, "sitool.send: invalid payload");
        return 0;
    }

    char disp[768];
    btoh(txbuf, txlen, disp, sizeof disp, ' ', 1);
    printf("TX >> %s\n", disp);

    unsigned char rxbuf[256];
    int rxlen = handler_send_raw(st, txbuf, txlen, rxbuf, sizeof rxbuf);

    if (rxlen > 0) {
        btoh(rxbuf, rxlen, disp, sizeof disp, ' ', 1);
        printf("RX << %s\n", disp);
        lua_pushstring(L, disp);
        lua_pushlstring(L, (const char *)rxbuf, rxlen);
        return 2;
    }

    printf("RX << (no response)\n");
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

/* sitool.utils.btoh(raw_string) */
static int l_utils_btoh(lua_State *L)
{
    size_t len;
    const char *raw = luaL_checklstring(L, 1, &len);
    char out[768];
    btoh((const unsigned char *)raw, len, out, sizeof out, ' ', 1);
    lua_pushstring(L, out);
    return 1;
}

/* sitool.utils.htob(hex_string) */
static int l_utils_htob(lua_State *L)
{
    const char *hexstr = luaL_checkstring(L, 1);
    unsigned char out[256];
    int n = htob(hexstr, out, sizeof out);
    if (n < 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, (const char *)out, n);
    return 1;
}

/* sitool.utils.hex(raw_string) */
static int l_utils_hexdump(lua_State *L)
{
    size_t len;
    const unsigned char *data =
        (const unsigned char *)luaL_checklstring(L, 1, &len);

    luaL_Buffer B;
    luaL_buffinit(L, &B);

    for (size_t off = 0; off < len; off += 16) {
        char line[128];
        int pos = snprintf(line, sizeof line, "  %04X  ", (unsigned)off);

        for (size_t j = 0; j < 16; j++) {
            if (off + j < len)
                pos += snprintf(line + pos, sizeof line - pos,
                                "%02X ", data[off + j]);
            else
                pos += snprintf(line + pos, sizeof line - pos, "   ");
            if (j == 7)
                pos += snprintf(line + pos, sizeof line - pos, " ");
        }

        pos += snprintf(line + pos, sizeof line - pos, " |");
        for (size_t j = 0; j < 16 && off + j < len; j++) {
            unsigned char c = data[off + j];
            line[pos++] = (c >= 0x20 && c <= 0x7E) ? c : '.';
        }
        line[pos++] = '|';
        line[pos++] = '\n';
        line[pos]   = '\0';

        luaL_addstring(&B, line);
    }

    luaL_pushresult(&B);
    return 1;
}

/* sitool.utils.atob(ascii_string) -> raw bytes */
static int l_utils_atob(lua_State *L)
{
    size_t len;
    const char *ascii = luaL_checklstring(L, 1, &len);
    unsigned char out[256];
    int n = atob(ascii, len, out, sizeof out);
    if (n < 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, (const char *)out, n);
    return 1;
}

/* sitool.utils.btoa(raw_string) -> printable ASCII (non-printable -> '.') */
static int l_utils_btoa(lua_State *L)
{
    size_t len;
    const char *raw = luaL_checklstring(L, 1, &len);
    char out[768];
    int n = btoa((const unsigned char *)raw, len, out, sizeof out);
    if (n < 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, out);
    return 1;
}

/* global printf(fmt, ...) */
static int l_printf(lua_State *L)
{
    int nargs = lua_gettop(L);
    if (nargs == 0) return 0;

    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);

    for (int i = 1; i <= nargs; i++)
        lua_pushvalue(L, i);

    if (lua_pcall(L, nargs, 1, 0) != LUA_OK) {
        fprintf(stderr, "printf: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    printf("%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

/* register sitool.* into a Lua state */
static void register_api(lua_State *L, sitool_t *st)
{
    lua_pushlightuserdata(L, st);
    lua_setfield(L, LUA_REGISTRYINDEX, ST_KEY);

    lua_newtable(L);

    lua_pushcfunction(L, l_send);
    lua_setfield(L, -2, "send");

    lua_newtable(L);
    lua_pushcfunction(L, l_utils_btoh);
    lua_setfield(L, -2, "btoh");
    lua_pushcfunction(L, l_utils_htob);
    lua_setfield(L, -2, "htob");
    lua_pushcfunction(L, l_utils_atob);
    lua_setfield(L, -2, "atob");
    lua_pushcfunction(L, l_utils_btoa);
    lua_setfield(L, -2, "btoa");
    lua_pushcfunction(L, l_utils_hexdump);
    lua_setfield(L, -2, "hex");
    lua_setfield(L, -2, "utils");

    lua_setglobal(L, "sitool");

    lua_pushcfunction(L, l_printf);
    lua_setglobal(L, "printf");
}

/* call H.callbacks.<name>() if it exists */
static void call_callback(lua_State *L, const char *name)
{
    lua_getglobal(L, "H");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

    lua_getfield(L, -1, "callbacks");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 3); return; }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[lua] %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
}

// Public load / unload / dispatch / help

int handler_load(sitool_t *st, const char *name)
{
    handler_unload(st);

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "error: could not create Lua state\n");
        return -1;
    }
    luaL_openlibs(L);
    register_api(L, st);

    char path[HANDLER_PATH_MAX];
    if (handler_resolve(name, path, sizeof path) < 0) {
        fprintf(stderr, "error: handler '%s' not found\n", name);
        lua_close(L);
        return -1;
    }

    if (luaL_loadfile(L, path) != LUA_OK) {
        fprintf(stderr, "[lua] %s\n", lua_tostring(L, -1));
        lua_close(L);
        return -1;
    }

    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        fprintf(stderr, "[lua] %s\n", lua_tostring(L, -1));
        lua_close(L);
        return -1;
    }

    if (!lua_istable(L, -1)) {
        fprintf(stderr, "[lua] handler must return a table\n");
        lua_close(L);
        return -1;
    }

    lua_setglobal(L, "H");

    st->L = L;
    strncpy(st->handler, name, sizeof st->handler);
    st->handler[sizeof st->handler - 1] = '\0';

    call_callback(L, "on_load");
    return 0;
}

void handler_unload(sitool_t *st)
{
    if (!st->L) return;

    call_callback(st->L, "on_unload");
    lua_close(st->L);
    st->L = NULL;
    st->handler[0] = '\0';
}

int handler_dispatch(sitool_t *st, const char *cmd,
                     int argc, char **argv)
{
    lua_State *L = st->L;
    if (!L) return 0;

    lua_getglobal(L, "H");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return 0; }

    lua_getfield(L, -1, "commands");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return 0; }

    lua_getfield(L, -1, cmd);
    if (!lua_istable(L, -1)) { lua_pop(L, 3); return 0; }

    lua_getfield(L, -1, "run");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 4); return 0; }

    char argstr[512] = "";
    size_t pos = 0;
    for (int i = 1; i < argc && pos < sizeof argstr - 2; i++) {
        if (i > 1) argstr[pos++] = ' ';
        size_t slen = strlen(argv[i]);
        if (pos + slen >= sizeof argstr) break;
        memcpy(argstr + pos, argv[i], slen);
        pos += slen;
    }
    argstr[pos] = '\0';

    if (pos > 0)
        lua_pushstring(L, argstr);
    else
        lua_pushnil(L);

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[lua] %s: %s\n", cmd, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 3);
    return 1;
}

void handler_help(sitool_t *st)
{
    if (!st->L) return;

    lua_State *L = st->L;
    lua_getglobal(L, "H");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

    lua_getfield(L, -1, "commands");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

    printf("\n  [%s]\n\n", st->handler);

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_istable(L, -1)) {
            const char *name = NULL;
            const char *help = "";

            lua_getfield(L, -1, "name");
            if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "help");
            if (lua_isstring(L, -1)) help = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (name) printf("  %-10s %s\n", name, help);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
}

int handler_send_raw(sitool_t *st, const unsigned char *data, int len,
                     unsigned char *resp, int resp_max)
{
    if (st->fd < 0) return -1;
    int w = serial_write(st->fd, data, len);
    if (w < 0) return -1;
    int r = serial_read(st->fd, resp, resp_max);
    return r;
}

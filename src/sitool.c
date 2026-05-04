#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "sitool.h"
#include "serial.h"
#include "handler.h"
#include "utils.h"

static int cmd__help(sitool_t*, int, char**);
static int cmd__exit(sitool_t*, int, char**);
static int cmd__open(sitool_t*, int, char**);
static int cmd__close(sitool_t*, int, char**);
static int cmd__set(sitool_t*, int, char**);
static int cmd__get(sitool_t*, int, char**);
static int cmd__raw(sitool_t*, int, char**);
static int cmd__use(sitool_t*, int, char**);

static cmd_entry_t commands[] = {
    { "help",  "Display help",                         		   cmd__help  },
    { "exit",  "Exit sitool",                          		   cmd__exit  },
    { "open",  "Open serial connection (ex. open /ttyUSB0)",   cmd__open  },
    { "close", "Close serial connection",              		   cmd__close },
    { "set",   "Set attribute  (set key value)",       		   cmd__set   },
    { "get",   "Get attribute  (get key)",             		   cmd__get   },
    { "raw",   "Send raw hex   (raw AA BB CC DD ...)", 		   cmd__raw   },
    { "use",   "Load handler (use <name> | list | none)", 	   cmd__use   },
    { NULL, NULL, NULL }
};

static void prompt_rebuild(sitool_t *st)
{
    if (st->fd >= 0 && st->port[0])
    {
        char tmp[128];
        strncpy(tmp, st->port, sizeof tmp);
        tmp[sizeof tmp - 1] = '\0';
        if (st->handler[0])
            snprintf(st->prompt, sizeof st->prompt, "%s (%s)> ",
            	basename(tmp), st->handler);
        else
            snprintf(st->prompt, sizeof st->prompt, "%s> ", basename(tmp));
    } else {

        if (st->handler[0])
            snprintf(st->prompt, sizeof st->prompt, "sitool (%s)> ",
                     st->handler);
        else
            strncpy(st->prompt, "sitool> ", sizeof st->prompt);
    }
    st->prompt[sizeof st->prompt - 1] = '\0';
}

static int cmd__help(sitool_t *st, int argc, char **argv)
{
	printf("\n");
    (void)argc; (void)argv;
    for (cmd_entry_t *e = commands; e->name; ++e)
        printf("  %-10s %s\n", e->name, e->help);
    printf("\n  Attributes: baudrate, databits, parity, stopbits, port\n");

    handler_help(st);
    printf("\n");
    return 0;
}

static int cmd__exit(sitool_t *st, int argc, char **argv)
{
    (void)argc; (void)argv;
    handler_unload(st);
    if (st->fd >= 0) {
        serial_close(st->fd);
        st->fd = -1;
    }
    return -1;
}

static int cmd__open(sitool_t *st, int argc, char **argv)
{
    (void)argc; (void)argv;

	if (argc>=2)
	{
		strncpy(st->port, argv[1], sizeof st->port);	        
	}

    if (st->port[0] == '\0') {
        printf("error: port not set (use: set port /dev/ttyUSB0)\n");
        return 0;
    }

    if (st->fd >= 0) {
        printf("closing previous connection\n");
        serial_close(st->fd);
        st->fd = -1;
    }

    st->fd = serial_open(st->port);
    if (st->fd < 0) {
        printf("error: could not open %s\n", st->port);
        return 0;
    }

    if (serial_config(st->fd, st->baudrate, st->databits,
                      st->parity, st->stopbits) < 0) {
        printf("error: could not configure port\n");
        serial_close(st->fd);
        st->fd = -1;
        return 0;
    }

    printf("opened %s @ %d %d%c%d\n",
           st->port, st->baudrate, st->databits, st->parity, st->stopbits);
    prompt_rebuild(st);
    return 0;
}

static int cmd__close(sitool_t *st, int argc, char **argv)
{
    (void)argc; (void)argv;

    if (st->fd < 0) {
        printf("not connected\n");
        return 0;
    }
    serial_close(st->fd);
    st->fd = -1;
    printf("closed\n");
    prompt_rebuild(st);
    return 0;
}

static int cmd__set(sitool_t *st, int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: set <key> <value>\n");
        return 0;
    }

    const char *key = argv[1];
    const char *val = argv[2];

    if (strcmp(key, "port") == 0) {
        strncpy(st->port, val, sizeof st->port);
        st->port[sizeof st->port - 1] = '\0';
    } else if (strcmp(key, "baudrate") == 0) {
        int b = atoi(val);
        if (serial_i2speed(b) == (speed_t)-1) {
            printf("error: unsupported baudrate %s\n", val);
            return 0;
        }
        st->baudrate = b;
    } else if (strcmp(key, "databits") == 0) {
        int d = atoi(val);
        if (d < 5 || d > 8) {
            printf("error: databits must be 5-8\n");
            return 0;
        }
        st->databits = d;
    } else if (strcmp(key, "parity") == 0) {
        char p = val[0];
        if (p >= 'a' && p <= 'z') p -= 32;
        if (p != 'N' && p != 'E' && p != 'O') {
            printf("error: parity must be N, E, or O\n");
            return 0;
        }
        st->parity = p;
    } else if (strcmp(key, "stopbits") == 0) {
        int s = atoi(val);
        if (s != 1 && s != 2) {
            printf("error: stopbits must be 1 or 2\n");
            return 0;
        }
        st->stopbits = s;
    } else {
        printf("unknown attribute: %s\n", key);
    }
    return 0;
}

static int cmd__get(sitool_t *st, int argc, char **argv)
{
    if (argc < 2) {
        printf("  port      %s\n", st->port[0] ? st->port : "(not set)");
        printf("  baudrate  %d\n", st->baudrate);
        printf("  databits  %d\n", st->databits);
        printf("  parity    %c\n", st->parity);
        printf("  stopbits  %d\n", st->stopbits);
        printf("  fd        %d\n", st->fd);
        printf("  handler   %s\n", st->handler[0] ? st->handler : "(none)");
        return 0;
    }

    const char *key = argv[1];

    if (strcmp(key, "port") == 0)
        printf("%s\n", st->port[0] ? st->port : "(not set)");
    else if (strcmp(key, "baudrate") == 0)
        printf("%d\n", st->baudrate);
    else if (strcmp(key, "databits") == 0)
        printf("%d\n", st->databits);
    else if (strcmp(key, "parity") == 0)
        printf("%c\n", st->parity);
    else if (strcmp(key, "stopbits") == 0)
        printf("%d\n", st->stopbits);
    else if (strcmp(key, "handler") == 0)
        printf("%s\n", st->handler[0] ? st->handler : "(none)");
    else
        printf("unknown attribute: %s\n", key);

    return 0;
}

static int cmd__raw(sitool_t *st, int argc, char **argv)
{
    if (st->fd < 0) {
        printf("error: not connected (use: open)\n");
        return 0;
    }

    if (argc < 2) {
        printf("usage: raw AA BB CC DD ...\n");
        return 0;
    }

    char hexbuf[512];
    size_t pos = 0;
    for (int i = 1; i < argc && pos < sizeof hexbuf - 3; i++) {
        if (i > 1) hexbuf[pos++] = ' ';
        size_t slen = strlen(argv[i]);
        if (pos + slen >= sizeof hexbuf) break;
        memcpy(hexbuf + pos, argv[i], slen);
        pos += slen;
    }
    hexbuf[pos] = '\0';

    unsigned char raw[256];
    int n = htob(hexbuf, raw, sizeof raw);
    if (n <= 0) {
        printf("error: invalid hex data\n");
        return 0;
    }

    char disp[768];
    btoh(raw, n, disp, sizeof disp, ' ', 1);
    printf("TX >> %s\n", disp);

    unsigned char resp[256];
    int r = handler_send_raw(st, raw, n, resp, sizeof resp);
    if (r > 0) {
        btoh(resp, r, disp, sizeof disp, ' ', 1);
        printf("RX << %s\n", disp);
    } else {
        printf("RX << (no response)\n");
    }

    return 0;
}

static int cmd__use(sitool_t *st, int argc, char **argv)
{
    if (argc < 2) {
        if (st->handler[0])
            printf("handler: %s\n", st->handler);
        else
            printf("no handler loaded\n");
        printf("usage: use <name> | list | none\n");
        return 0;
    }

    if (strcmp(argv[1], "none") == 0) {
        if (!st->L) {
            printf("no handler loaded\n");
            return 0;
        }
        printf("unloading handler '%s'\n", st->handler);
        handler_unload(st);
        prompt_rebuild(st);
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        printf("Available handlers:\n");
        handler_list_print();
        return 0;
    }

    if (handler_load(st, argv[1]) < 0) {
        printf("error: could not load handler '%s'\n", argv[1]);
    } else {
        prompt_rebuild(st);
    }
    return 0;
}


void sitool_init(sitool_t *st)
{
    if (!st) return;
    memset(st, 0, sizeof *st);
    st->fd       = -1;
    st->baudrate = 9600;
    st->databits = 8;
    st->parity   = 'N';
    st->stopbits = 1;
    st->L        = NULL;
    prompt_rebuild(st);
}

int sitool_eval(sitool_t *st, char *line)
{
    if (!line) return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return 0;

    char *argv[64];
    int argc = parse_args(line, argv);
    if (argc == 0) return 0;

    /* try built-in commands first */
    for (cmd_entry_t *e = commands; e->name; ++e) {
        if (strcmp(argv[0], e->name) == 0)
            return e->cmd(st, argc, argv);
    }

    /* try lua handler commands */
    if (st->L && handler_dispatch(st, argv[0], argc, argv))
        return 0;

    printf("unknown command: %s\n", argv[0]);
    return 0;
}

void sitool_repl(sitool_t *st)
{

	printf(" ___	                                      \n");											
	printf("| _ )  	                                      \n");
	printf("| _ \\ 	                                      \n");
	printf("|___/eemo's                                   \n");
	printf(". . . . . . sitool                            \n");
	printf(". . . . . . Serial Interface Toolkit %s       \n",SITOOL_VERSION);
	printf(". . . . . . nflatrea@mailo.com <Noë Flatreaud>\n\n");

    printf("Type 'help' for help.\n\n");

    char *line;
    int   ret;

	prompt_rebuild(st);

    while ((line = readline(st->prompt)) != NULL) {
    	if (*line) add_history(line);
        ret = sitool_eval(st, line);
        free(line);
        if (ret < 0) break;
	}
}

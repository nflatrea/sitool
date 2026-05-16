#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <libgen.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "sitool.h"
#include "serial.h"
#include "handler.h"
#include "mode.h"
#include "utils.h"

enum attr_type { ATTR_STR, ATTR_INT, ATTR_CHAR, ATTR_BOOL, ATTR_SIGNAL };

typedef struct
{
    const char   *name;
    enum attr_type type;
    size_t         offset;   /* offsetof into sitool_t              */
    int            lo, hi;   /* valid range for ATTR_INT            */
    const char    *chars;    /* valid chars for ATTR_CHAR (e.g "NEO") */
    int            sig;      /* SIG_DTR or SIG_RTS for ATTR_SIGNAL  */
    int            reconfig; /* 1 = call reconfig_live after set    */
} attr_entry_t;

#define OFF(f) offsetof(sitool_t, f)

static const attr_entry_t attrs[] = {
    { "port",     ATTR_STR,    OFF(port),     0, 0, NULL, 0, 0 },
    { "baudrate", ATTR_INT,    OFF(baudrate), 0, 0, NULL, 0, 1 },
    { "databits", ATTR_INT,    OFF(databits), 5, 8, NULL, 0, 1 },
    { "parity",   ATTR_CHAR,   OFF(parity),   0, 0, "NEO",0, 1 },
    { "stopbits", ATTR_INT,    OFF(stopbits), 1, 2, NULL, 0, 1 },
    { "echo",     ATTR_BOOL,   OFF(echo),     0, 0, NULL, 0, 0 },
    { "dtr",      ATTR_SIGNAL, OFF(dtr),      0, 0, NULL, SIG_DTR, 0 },
    { "rts",      ATTR_SIGNAL, OFF(rts),      0, 0, NULL, SIG_RTS, 0 },
    { NULL, 0, 0, 0, 0, NULL, 0, 0 }
};

static int reconfig_live(sitool_t *st);
static int parse_bool(const char *val);

static int cmd__help(sitool_t*, int, char**);
static int cmd__exit(sitool_t*, int, char**);
static int cmd__open(sitool_t*, int, char**);
static int cmd__close(sitool_t*, int, char**);
static int cmd__set(sitool_t*, int, char**);
static int cmd__raw(sitool_t*, int, char**);
static int cmd__use(sitool_t*, int, char**);
static int cmd__list(sitool_t*, int, char**);
static int cmd__term(sitool_t*, int, char**);
static int cmd__sniff(sitool_t*, int, char**);

static cmd_entry_t commands[] = {
    { "help",  "Display help",                              cmd__help  },
    { "exit",  "Exit sitool",                               cmd__exit  },
    { "open",  "Open serial connection (ex. open /ttyUSB0)",cmd__open  },
    { "close", "Close serial connection",                   cmd__close },
    { "set",   "Set attribute  (set key value)",            cmd__set   },
    { "raw",   "Send raw payload (raw AA BB \"TXT\" ...)",  cmd__raw   },
    { "use",   "Load handler (use <name> | none)",          cmd__use   },
    { "list",  "List (options | devices | handlers | all)",  cmd__list  },
    { "term",  "Raw terminal (term [port]) Ctrl-] to quit", cmd__term  },
    { "sniff", "Passive listen (sniff [port]) Ctrl-] to quit",cmd__sniff},
    { NULL, NULL, NULL }
};

static void prompt_rebuild(sitool_t *st)
{
    const char *port = NULL;

    if (st->fd >= 0 && st->port[0]) {
        char tmp[128];
        strncpy(tmp, st->port, sizeof tmp);
        tmp[sizeof tmp - 1] = '\0';
        port = basename(tmp);
    }

    if (port && st->handler[0])
        snprintf(st->prompt, sizeof st->prompt, "%s (%s)> ",
                 port, st->handler);
    else if (port)
        snprintf(st->prompt, sizeof st->prompt, "%s> ", port);
    else if (st->handler[0])
        snprintf(st->prompt, sizeof st->prompt, "sitool (%s)> ",
                 st->handler);
    else
        snprintf(st->prompt, sizeof st->prompt, "sitool> ");
}

static int parse_bool(const char *val)
{
    if (strcmp(val, "on") == 0 || strcmp(val, "1") == 0) return 1;
    if (strcmp(val, "off") == 0 || strcmp(val, "0") == 0) return 0;
    return -1;
}

static int reconfig_live(sitool_t *st)
{
    if (st->fd < 0) return 0;
    if (serial_config(st->fd, st->baudrate, st->databits,
                      st->parity, st->stopbits) < 0) {
        printf("warning: could not apply settings on live port\n");
        return -1;
    }
    printf("applied: %d %d%c%d\n",
           st->baudrate, st->databits, st->parity, st->stopbits);
    return 0;
}

static int cmd__help(sitool_t *st, int argc, char **argv)
{
    (void)argc; (void)argv;

    printf("\n");
    for (cmd_entry_t *e = commands; e->name; ++e)
        printf("  %-10s %s\n", e->name, e->help);

    printf("\n  Attributes:");
    for (const attr_entry_t *a = attrs; a->name; ++a)
        printf(" %s", a->name);
    printf("\n");

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
    if (argc >= 2) {
        strncpy(st->port, argv[1], sizeof st->port);
        st->port[sizeof st->port - 1] = '\0';
    }

    if (st->port[0] == '\0') {
        printf("error: port not set (use: set port /dev/ttyUSB0)\n");
        return 0;
    }

    if (st->fd >= 0) {
        printf("closing %s\n", st->port);
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

    /* guard: re-ensure stdout has sane output processing */
    if (isatty(STDOUT_FILENO)) {
        struct termios tout;
        if (tcgetattr(STDOUT_FILENO, &tout) == 0) {
            tout.c_oflag |= OPOST | ONLCR;
            tcsetattr(STDOUT_FILENO, TCSANOW, &tout);
        }
    }

    printf("opened %s @ %d %d%c%d\n",
           st->port, st->baudrate, st->databits, st->parity, st->stopbits);

    if (st->dtr >= 0) serial_signal(st->fd, SIG_DTR, st->dtr);
    if (st->rts >= 0) serial_signal(st->fd, SIG_RTS, st->rts);

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

    for (const attr_entry_t *a = attrs; a->name; ++a) {
        if (strcmp(key, a->name) != 0) continue;

        char *base = (char *)st;

        switch (a->type) {
        case ATTR_STR:
            strncpy(base + a->offset, val, 128);
            base[a->offset + 127] = '\0';
            break;

        case ATTR_INT: {
            int v = atoi(val);
            /* baudrate: validate via serial_i2speed */
            if (strcmp(a->name, "baudrate") == 0) {
                if (serial_i2speed(v) == (speed_t)-1) {
                    printf("error: unsupported baudrate %s\n", val);
                    return 0;
                }
            } else if (a->lo || a->hi) {
                if (v < a->lo || v > a->hi) {
                    printf("error: %s must be %d-%d\n", a->name, a->lo, a->hi);
                    return 0;
                }
            }
            *(int *)(base + a->offset) = v;
            break;
        }

        case ATTR_CHAR: {
            char c = val[0];
            if (c >= 'a' && c <= 'z') c -= 32;
            if (!a->chars || !strchr(a->chars, c)) {
                printf("error: %s must be one of: %s\n", a->name, a->chars);
                return 0;
            }
            *(char *)(base + a->offset) = c;
            break;
        }

        case ATTR_BOOL: {
            int v = parse_bool(val);
            if (v < 0) {
                printf("error: %s must be on or off\n", a->name);
                return 0;
            }
            *(int *)(base + a->offset) = v;
            break;
        }

        case ATTR_SIGNAL: {
            int v = parse_bool(val);
            if (v < 0) {
                printf("error: %s must be on or off\n", a->name);
                return 0;
            }
            *(int *)(base + a->offset) = v;
            if (st->fd >= 0 && serial_signal(st->fd, a->sig, v) < 0)
                printf("warning: signal control not supported on this device\n");
            break;
        }
        }

        if (a->reconfig)
            reconfig_live(st);
        return 0;
    }

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
        printf("       raw \"HELLO\"            (ASCII)\n");
        printf("       raw 02 10 \"HELLO\" FF   (mixed)\n");
        return 0;
    }

    char payload[SITOOL_LINE];
    size_t pos = 0;
    for (int i = 1; i < argc && pos < sizeof payload - 3; i++) {
        if (i > 1) payload[pos++] = ' ';
        size_t slen = strlen(argv[i]);
        if (pos + slen >= sizeof payload) break;
        memcpy(payload + pos, argv[i], slen);
        pos += slen;
    }
    payload[pos] = '\0';

    unsigned char raw[SITOOL_MTU];
    int n = parse_payload(payload, raw, sizeof raw);
    if (n <= 0) {
        printf("error: invalid payload\n");
        return 0;
    }

    char disp[SITOOL_DISP];
    btoh(raw, n, disp, sizeof disp, ' ', 1);
    printf("TX >> %s\n", disp);

    unsigned char resp[SITOOL_MTU];
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
        printf("usage: use <name> | none\n");
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

    if (handler_load(st, argv[1]) < 0) {
        printf("error: could not load handler '%s'\n", argv[1]);
    } else {
        prompt_rebuild(st);
    }
    return 0;
}

static void list_options(sitool_t *st)
{
    char *base = (char *)st;

    for (const attr_entry_t *a = attrs; a->name; ++a) {
        printf("  %-10s ", a->name);
        switch (a->type) {
        case ATTR_STR: {
            const char *s = base + a->offset;
            printf("%s\n", *s ? s : "(not set)");
            break;
        }
        case ATTR_INT:
            printf("%d\n", *(int *)(base + a->offset));
            break;
        case ATTR_CHAR:
            printf("%c\n", *(char *)(base + a->offset));
            break;
        case ATTR_BOOL:
            printf("%s\n", *(int *)(base + a->offset) ? "on" : "off");
            break;
        case ATTR_SIGNAL: {
            int v = *(int *)(base + a->offset);
            printf("%s\n", v < 0 ? "(auto)" : v ? "on" : "off");
            break;
        }
        }
    }
    printf("  %-10s %d\n", "fd", st->fd);
    printf("  %-10s %s\n", "handler",
           st->handler[0] ? st->handler : "(none)");
}

static void list_devices(void)
{
    serial_dev_t devs[SERIAL_DEV_MAX];
    int n = serial_list(devs, SERIAL_DEV_MAX);
    if (n == 0) {
        printf("  (no serial devices found)\n");
        return;
    }
    for (int i = 0; i < n; i++)
        printf("  %s\n", devs[i].path);
}

static int cmd__list(sitool_t *st, int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: list options | devices | handlers | all\n");
        return 0;
    }

    if (strcmp(argv[1], "options") == 0) {
        list_options(st);
        return 0;
    }

    if (strcmp(argv[1], "devices") == 0) {
        list_devices();
        return 0;
    }

    if (strcmp(argv[1], "handlers") == 0) {
        handler_list_print();
        return 0;
    }

    if (strcmp(argv[1], "all") == 0) {
        printf("[options]\n");
        list_options(st);
        printf("\n[devices]\n");
        list_devices();
        printf("\n[handlers]\n");
        handler_list_print();
        return 0;
    }

    printf("unknown: list %s\n", argv[1]);
    printf("usage: list options | devices | handlers | all\n");
    return 0;
}

static void term_on_rx(sitool_t *st, const unsigned char *buf, int len)
{
    (void)st;
    ssize_t wr = write(STDOUT_FILENO, buf, len);
    (void)wr;
}

static void term_on_key(sitool_t *st, unsigned char c)
{
    ssize_t wr = write(st->fd, &c, 1);
    if (st->echo)
        wr = write(STDOUT_FILENO, &c, 1);
    (void)wr;
}

static void sniff_on_rx(sitool_t *st, const unsigned char *buf, int len)
{
    if (st->L) {
        handler_on_recv(st, buf, len);
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);

    /* header line */
    char line[128];
    int  pos;

    pos = snprintf(line, sizeof line, "[%02d:%02d:%02d.%03ld] len=%d\r\n",
                   tm->tm_hour, tm->tm_min, tm->tm_sec,
                   tv.tv_usec / 1000, len);
    ssize_t wr = write(STDOUT_FILENO, line, pos);

    /* hexdump lines — one write per 16-byte row */
    for (int offset = 0; offset < len; offset += 16) {
        int row = (len - offset < 16) ? (len - offset) : 16;
        pos = 0;

        line[pos++] = ' '; line[pos++] = ' ';
        for (int i = 0; i < 16; i++) {
            if (i < row) {
                static const char hex[] = "0123456789abcdef";
                line[pos++] = hex[buf[offset + i] >> 4];
                line[pos++] = hex[buf[offset + i] & 0x0F];
                line[pos++] = ' ';
            } else {
                line[pos++] = ' ';
                line[pos++] = ' ';
                line[pos++] = ' ';
            }
        }
        line[pos++] = ' '; line[pos++] = '|';
        for (int i = 0; i < row; i++) {
            unsigned char c = buf[offset + i];
            line[pos++] = (c >= 32 && c < 127) ? c : '.';
        }
        line[pos++] = '|';
        line[pos++] = '\r'; line[pos++] = '\n';

        wr = write(STDOUT_FILENO, line, pos);
    }
    (void)wr;
}

static int enter_term(sitool_t *st, int argc, char **argv, int interactive)
{
    if (argc >= 2) {
        strncpy(st->port, argv[1], sizeof st->port);
        st->port[sizeof st->port - 1] = '\0';
    }

    if (st->fd < 0 && st->port[0]) {
        char open_cmd[] = "open";
        sitool_eval(st, open_cmd);
    }

    if (st->fd < 0) {
        printf("error: not connected (use: open or set port)\n");
        return 0;
    }

    const char *label = interactive ? "term" : "sniff";

    if (interactive)
        printf("\r\n--- %s (%s @ %d %d%c%d) | echo %s | Ctrl-] to quit ---\r\n",
               label, st->port, st->baudrate, st->databits, st->parity,
               st->stopbits, st->echo ? "on" : "off");
    else
        printf("\r\n--- %s (%s @ %d %d%c%d)%s | Ctrl-] to quit ---\r\n",
               label, st->port, st->baudrate, st->databits, st->parity,
               st->stopbits, st->L ? " [handler]" : "");

    mode_opts_t opts = {
        .timeout_ms = -1,
        .on_rx      = interactive ? term_on_rx : sniff_on_rx,
        .on_key     = interactive ? term_on_key : NULL,
        .on_tick    = NULL,
    };
    mode_run(st, &opts);

    printf("\r\n--- %s closed ---\r\n", label);
    return 0;
}

static int cmd__term(sitool_t *st, int argc, char **argv)
{
    return enter_term(st, argc, argv, 1);
}

static int cmd__sniff(sitool_t *st, int argc, char **argv)
{
    return enter_term(st, argc, argv, 0);
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
    st->echo     = 0;
    st->dtr      = -1;
    st->rts      = -1;
    st->L        = NULL;
    prompt_rebuild(st);
}

int sitool_eval(sitool_t *st, char *line)
{
    if (!line) return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return 0;

    char *argv[SITOOL_ARGV_MAX];
    int argc = parse_args(line, argv, SITOOL_ARGV_MAX);
    if (argc == 0) return 0;

    for (cmd_entry_t *e = commands; e->name; ++e) {
        if (strcmp(argv[0], e->name) == 0)
            return e->cmd(st, argc, argv);
    }

    if (st->L && handler_dispatch(st, argv[0], argc, argv))
        return 0;

    printf("unknown command: %s\n", argv[0]);
    return 0;
}

void sitool_repl(sitool_t *st)
{
    printf(" ___\t                                      \n");
    printf("| _ )  \t                                      \n");
    printf("| _ \\ \t                                      \n");
    printf("|___/eemo's                                   \n");
    printf(". . . . . . sitool                            \n");
    printf(". . . . . . Serial Interface Toolkit %s       \n", SITOOL_VERSION);
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

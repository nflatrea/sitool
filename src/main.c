#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sitool.h"
#include "handler.h"

static void usage(const char *prog)
{
    printf("Serial Interface Toolkit %s\n\n", SITOOL_VERSION);
    printf("Usage: %s [-p PORT] [-b BAUDRATE] [-u HANDLER] [-h] [CMD ...]\n\n", prog);
    printf("  -p, --port      PORT       serial port  (e.g. /dev/ttyUSB0)\n");
    printf("  -b, --baudrate  BAUDRATE   baud rate    (e.g. 9600, 115200)\n");
    printf("  -H, --handler   HANDLER    load Lua handler on start\n");
    printf("  -h, --help                 display this help\n\n");
    printf("If CMD is given, execute it and exit.\n");
    printf("Otherwise enter interactive mode.\n");
}

int main(int argc, char **argv)
{
    sitool_t st;
    sitool_init(&st);

    const char *handler = NULL;
    int cmd_start = 0;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-p") == 0 ||
            strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", argv[i]);
                return 1;
            }
            strncpy(st.port, argv[++i], sizeof st.port);
            st.port[sizeof st.port - 1] = '\0';
            continue;
        }

        if (strcmp(argv[i], "-b") == 0 ||
            strcmp(argv[i], "--baudrate") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", argv[i]);
                return 1;
            }
            st.baudrate = atoi(argv[++i]);
            continue;
        }

        if (strcmp(argv[i], "-H") == 0 ||
            strcmp(argv[i], "--handler") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", argv[i]);
                return 1;
            }
            handler = argv[++i];
            continue;
        }

        /* first non-option arg, start of CMD */
        cmd_start = i;
        break;
    }

    /* open port if given */
    if (st.port[0]) {
        char open_cmd[] = "open";
        sitool_eval(&st, open_cmd);
        if (st.fd < 0) return 1;
    }

    /* load handler if given */
    if (handler) {
        if (handler_load(&st, handler) < 0)
            fprintf(stderr, "warning: could not load handler '%s'\n", handler);
    }

    /* trailing CMD: eval and exit */
    if (cmd_start) {
        char line[512];
        size_t pos = 0;
        for (int i = cmd_start; i < argc && pos < sizeof line - 2; i++) {
            if (i > cmd_start) line[pos++] = ' ';
            size_t slen = strlen(argv[i]);
            if (pos + slen >= sizeof line) break;
            memcpy(line + pos, argv[i], slen);
            pos += slen;
        }
        line[pos] = '\0';
        sitool_eval(&st, line);

        handler_unload(&st);
        if (st.fd >= 0) {
            char close_cmd[] = "close";
            sitool_eval(&st, close_cmd);
        }
        return 0;
    }

    /* interactive mode */
    sitool_repl(&st);
    return 0;
}

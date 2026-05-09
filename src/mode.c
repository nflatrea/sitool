#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <termios.h>

#include "mode.h"

int mode_run(sitool_t *st, const mode_opts_t *opts)
{
    if (st->fd < 0) {
        printf("error: not connected\n");
        return -1;
    }

    struct termios orig;
    if (tcgetattr(STDIN_FILENO, &orig) != 0) {
        printf("error: tcgetattr failed\n");
        return -1;
    }

    struct termios raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    raw.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR);
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    struct pollfd fds[2];
    fds[0].fd     = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd     = st->fd;
    fds[1].events = POLLIN;

    int running = 1;

    while (running) {
        int ret = poll(fds, 2, opts->timeout_ms);

        if (ret < 0) break;

        if (ret == 0) {
            if (opts->on_tick)
                opts->on_tick(st);
            continue;
        }

        if (fds[1].revents & POLLIN) {
            unsigned char buf[256];
            ssize_t n = read(st->fd, buf, sizeof buf);
            if (n <= 0) break;
            if (opts->on_rx)
                opts->on_rx(st, buf, (int)n);
        }

        if (fds[0].revents & POLLIN) {
            unsigned char c;
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n <= 0) break;
            if (c == 0x1D) { running = 0; break; }
            if (opts->on_key)
                opts->on_key(st, c);
        }

        if ((fds[1].revents & (POLLHUP | POLLERR)) ||
            (fds[0].revents & (POLLHUP | POLLERR)))
            break;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    return 0;
}

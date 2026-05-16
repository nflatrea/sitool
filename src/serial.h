#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stddef.h>
#include <sys/ioctl.h>

#define SERIAL_DEV_MAX 64

#define SIG_DTR  TIOCM_DTR   /* 0x002 */
#define SIG_RTS  TIOCM_RTS   /* 0x004 */

typedef struct
{
    char path[128];

} serial_dev_t;

speed_t serial_i2speed(int baud);
int 	serial_open(const char *port);
void 	serial_close(int fd);
int 	serial_config(int fd, int baud, int databits, char parity, int stopbits);
int 	serial_write(int fd, const void *buf, size_t len);
int 	serial_read(int fd, void *buf, size_t len);
int 	serial_poll(int fd, void *buf, size_t len, int timeout_ms);
int 	serial_list(serial_dev_t *list, int max);
int 	serial_signal(int fd, int sig, int state);
int 	serial_get_signal(int fd, int sig);

#endif // SERIAL_H

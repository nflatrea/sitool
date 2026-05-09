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

/*

serial_i2speed

  Convert integer baud rate to speed_t.
  Returns (speed_t)-1 on unsupported values.

serial_open

  Open serial connection based on port string
  Returns fd or -1

serial_close

  Close serial connection from fd
  if fd<0 no nothing

serial_config

  Configure serial port.

    baud            integer baud rate (9600, 115200, …)
    databits        5, 6, 7, or 8
    parity         'N' (none), 'E' (even), 'O' (odd)
    stopbits       1 or 2

  Returns 0 on success, -1 on error.

serial_write	

  Write `len` bytes from `buf` to the serial port.
  Returns number of bytes written, or -1 on error.

serial_read

  Read up to `len` bytes into `buf` from the serial port.
  Blocks until data arrives or timeout (VTIME).
  Returns number of bytes read, 0 on timeout, or -1 on error.

serial_poll

  Non-blocking read: poll for data with a timeout in milliseconds.
  Returns number of bytes read, 0 if nothing available, or -1 on error.

serial_list

  List available serial devices (e.g. /dev/ttyUSB*, /dev/ttyACM*).
  Fills `list` with up to `max` entries.
  Returns the number of devices found.

serial_signal

  Set modem line signal. sig is SIG_DTR or SIG_RTS.
  state: 1 = assert (high), 0 = deassert (low).
  Returns 0 on success, -1 on error.

serial_get_signal

  Get current state of a modem line signal.
  Returns 1 if asserted, 0 if not, -1 on error.

*/

#endif // SERIAL_H

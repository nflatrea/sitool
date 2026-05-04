#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stddef.h>

speed_t serial_i2speed(int baud);
int 	serial_open(const char *port);
void 	serial_close(int fd);
int 	serial_config(int fd, int baud, int databits, char parity, int stopbits);
int 	serial_write(int fd, const void *buf, size_t len);
int 	serial_read(int fd, void *buf, size_t len);

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
  Returns number of bytes read, 0 on timeout, or -1 on error.

*/

#endif // SERIAL_H

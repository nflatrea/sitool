#include "serial.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

speed_t serial_i2speed(int baud)
{
	// Convert integer to speed_t	
	// J'ai pas trouvé mieux pour l'instant...
	
    switch (baud) {
    case 0:       return B0;
    case 50:      return B50;
    case 75:      return B75;
    case 110:     return B110;
    case 134:     return B134;
    case 150:     return B150;
    case 200:     return B200;
    case 300:     return B300;
    case 600:     return B600;
    case 1200:    return B1200;
    case 1800:    return B1800;
    case 2400:    return B2400;
    case 4800:    return B4800;
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 500000:  return B500000;
    case 576000:  return B576000;
    case 921600:  return B921600;
    case 1000000: return B1000000;
    case 1152000: return B1152000;
    case 1500000: return B1500000;
    case 2000000: return B2000000;
    case 2500000: return B2500000;
    case 3000000: return B3000000;
    case 3500000: return B3500000;
    case 4000000: return B4000000;
    default: return (speed_t)-1;
    }
}

int serial_open(const char *port)
{
	
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("serial_open");
        return -1;
    }
    /* clear the non-blocking flag so reads can block/timeout normally */
    fcntl(fd, F_SETFL, 0);
    return fd;
}

int serial_config(int fd, int baud, int databits, char parity, int stopbits)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        perror("serial_config: tcgetattr");
        return -1;
    }

    speed_t speed = serial_i2speed(baud);
    if (speed == (speed_t)-1) {
        fprintf(stderr, "serial_config: unsupported baud rate %d\n", baud);
        return -1;
    }
    
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag &= ~CSIZE;
    switch (databits)
    {
    	case 5: tty.c_cflag |= CS5; break;
    	case 6: tty.c_cflag |= CS6; break;
    	case 7: tty.c_cflag |= CS7; break;
    	case 8: tty.c_cflag |= CS8; break;
    	default:
        	fprintf(stderr, "serial_config: unsupported databits %d\n", databits);
        return -1;
    }

    switch (parity)
    {
    	case 'N': case 'n':
        	tty.c_cflag &= ~PARENB;
        	break;
    	case 'E': case 'e':
        	tty.c_cflag |= PARENB;
        	tty.c_cflag &= ~PARODD;
        	break;
    	case 'O': case 'o':
        	tty.c_cflag |= PARENB;
        	tty.c_cflag |= PARODD;
        	break;
    default:
        	fprintf(stderr, "serial_config: unsupported parity '%c'\n", parity);
        	return -1;
    }

    if (stopbits == 2) tty.c_cflag |= CSTOPB;
    else tty.c_cflag &= ~CSTOPB;

    tty.c_cflag &= ~CRTSCTS;          /* no hw flow control          */
    tty.c_cflag |=  CLOCAL | CREAD;   /* ignore modem, enable rx     */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* raw input    */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);          /* no sw flow  */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
                      ISTRIP | INLCR | IGNCR | ICRNL); /* raw input */
                      
    tty.c_oflag &= ~OPOST;            /* raw output                  */

    /* return after 1 s timeout or 1+ bytes */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 10;             /* 10 × 0.1 s = 1 s*/

    tcflush(fd, TCIOFLUSH);
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("serial_config: tcsetattr");
        return -1;
    }

    return 0;
}

int serial_write(int fd, const void *buf, size_t len)
{
    ssize_t n = write(fd, buf, len);
    if (n == -1) perror("serial_write");
    return (int)n;
}

int serial_read(int fd, void *buf, size_t len)
{
    ssize_t n = read(fd, buf, len);
    if (n == -1) perror("serial_read");
    return (int)n;
}

void serial_close(int fd)
{
    if (fd >= 0) close(fd);
}

int serial_poll(int fd, void *buf, size_t len, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd     = fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        perror("serial_poll");
        return -1;
    }
    if (ret == 0) return 0; /* timeout, nothing available */

    if (pfd.revents & POLLIN) {
        ssize_t n = read(fd, buf, len);
        if (n == -1) { perror("serial_poll: read"); return -1; }
        return (int)n;
    }

    return 0;
}

int serial_signal(int fd, int sig, int state)
{
    if (fd < 0) return -1;
    int req = state ? TIOCMBIS : TIOCMBIC;
    if (ioctl(fd, req, &sig) < 0)
        return -1;
    return 0;
}

int serial_get_signal(int fd, int sig)
{
    if (fd < 0) return -1;
    int status;
    if (ioctl(fd, TIOCMGET, &status) < 0)
        return -1;
    return (status & sig) ? 1 : 0;
}

int serial_list(serial_dev_t *list, int max)
{
    int count = 0;
    DIR *d = opendir("/dev");
    if (!d) return 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        const char *name = ent->d_name;

        /* match ttyUSB*, ttyACM*, ttyS*, ttyAMA* */
        if (strncmp(name, "ttyUSB", 6) == 0 ||
            strncmp(name, "ttyACM", 6) == 0 ||
            strncmp(name, "ttyAMA", 6) == 0 )
            // strncmp(name, "ttyS",   4) == 0)		Garbage info but might be usefull....
        {
            snprintf(list[count].path, sizeof list[count].path,
                     "/dev/%s", name);
            count++;
        }
    }
    closedir(d);
	
    /* also scan /dev/pts for pseudo-terminals (useful for testing) */
	/* TODO: REMOVE, garbage info*/
    // d = opendir("/dev/pts");
    // if (d) {
    //     while ((ent = readdir(d)) != NULL && count < max) {
    //         const char *name = ent->d_name;
    //         if (name[0] == '.' || strcmp(name, "ptmx") == 0)
    //             continue;
    //         /* only numeric entries */
    //         int is_num = 1;
    //         for (const char *c = name; *c; c++) {
    //             if (*c < '0' || *c > '9') { is_num = 0; break; }
    //         }
    //         if (!is_num) continue;
    //         snprintf(list[count].path, sizeof list[count].path,
    //                  "/dev/pts/%s", name);
    //         count++;
    //     }
    //     closedir(d);
    // }

    return count;
}

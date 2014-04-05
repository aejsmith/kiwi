/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Terminal control functions.
 */

#ifndef __TERMIOS_H
#define __TERMIOS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size of termios control character array. */
#define NCCS			32

/** POSIX termios structure types. */
typedef unsigned char cc_t;		/**< Control character type. */
typedef unsigned int  speed_t;		/**< Terminal speed type. */
typedef unsigned int  tcflag_t;		/**< Terminal control flag type. */

/** Terminal settings structure. */
struct termios {
	tcflag_t c_iflag;		/**< Input modes. */
	tcflag_t c_oflag;		/**< Output modes. */
	tcflag_t c_cflag;		/**< Control modes. */
	tcflag_t c_lflag;		/**< Local modes. */
	cc_t     c_cc[NCCS];		/**< Control characters. */
	speed_t  c_ispeed;		/**< Input speed. */
	speed_t  c_ospeed;		/**< Output speed. */
};

/** Terminal size information structure. */
struct winsize {
        unsigned short ws_row;		/**< Number of rows. */
        unsigned short ws_col;		/**< Number of columns. */
};

/** Terminal control characters. */
#define VEOF			0	/**< EOF character. */
#define VEOL			1	/**< EOL character. */
#define VERASE			2	/**< ERASE character. */
#define VINTR			3	/**< INTR character. */
#define VKILL			4	/**< KILL character. */
#define VMIN			5	/**< MIN value. */
#define VQUIT			6	/**< QUIT character. */
#define VSTART			7	/**< START character. */
#define VSTOP			8	/**< STOP character. */
#define VSUSP			9	/**< SUSP character. */
#define VTIME			10	/**< TIME value. */
#define VLNEXT			12	/**< Escapes the next character. */
#define _POSIX_VDISABLE		0	/**< Control character is disabled. */

/** Input control flags (c_iflag). */
#define BRKINT			(1<<0)	/**< Signal interrupt on break. */
#define ICRNL			(1<<1)	/**< Map CR to NL on input. */
#define IGNBRK			(1<<2)	/**< Ignore break condition. */
#define IGNCR			(1<<3)	/**< Ignore CR. */
#define IGNPAR			(1<<4)	/**< Ignore characters with parity errors. */
#define INLCR			(1<<5)	/**< Map NL to CR on input. */
#define INPCK			(1<<6)	/**< Enable input parity check. */
#define ISTRIP			(1<<7)	/**< Strip character. */
#define IXANY			(1<<8)	/**< Enable any character to restart output. */
#define IXOFF			(1<<9)	/**< Enable start/stop input control. */
#define IXON			(1<<10)	/**< Enable start/stop output control. */
#define PARMRK			(1<<11)	/**< Mark parity errors. */

/** Output control flags (c_oflag). */
#define OPOST			(1<<0)	/**< Post-process output. */
#define ONLCR			(1<<1)	/**< Map NL to CR-NL on output. */
#define OCRNL			(1<<2)	/**< Map CR to NL on output. */
#define ONOCR			(1<<3)	/**< No CR output at column 0. */
#define ONLRET			(1<<4)	/**< NL performs CR function. */
#define OFILL			(1<<5)	/**< Use fill characters for delay. */

/** Control modes (c_cflag). */
#define CSIZE			0x0003	/**< Character size. */
#define   CS5			0x0000	/**<   5-bits. */
#define   CS6			0x0001	/**<   6-bits. */
#define   CS7			0x0002	/**<   7-bits. */
#define   CS8			0x0003	/**<   8-bits. */
#define CSTOPB			(1<<2)	/**< Send two stop bits, else one. */
#define CREAD			(1<<3)	/**< Enable receiver. */
#define PARENB			(1<<4)	/**< Parity enable. */
#define PARODD			(1<<5)	/**< Odd parity, else even. */
#define HUPCL			(1<<6)	/**< Hang up on last close. */
#define CLOCAL			(1<<7)	/**< Ignore modem status lines. */

/** Local modes (c_lflag). */
#define ECHO			(1<<0)	/**< Enable echo. */
#define ECHOE			(1<<1)	/**< Echo erase character as error-correcting backspace. */
#define ECHOK			(1<<2)	/**< Echo KILL. */
#define ECHONL			(1<<3)	/**< Echo NL. */
#define ICANON			(1<<4)	/**< Canonical input (erase and kill processing). */
#define IEXTEN			(1<<5)	/**< Enable extended input character processing. */
#define ISIG			(1<<6)	/**< Enable signals. */
#define NOFLSH			(1<<7)	/**< Disable flush after interrupt or quit. */
#define TOSTOP			(1<<8)	/**< Send SIGTTOU for background output. */

/** Baud rate flags (c_ispeed/c_ospeed). */
#define B0			0	/**< Hang up. */
#define B50			1	/**< 50 baud. */
#define B75			2	/**< 75 baud. */
#define B110			3	/**< 110 baud. */
#define B134			4	/**< 134.5 baud. */
#define B150			5	/**< 150 baud. */
#define B200			6	/**< 200 baud. */
#define B300			7	/**< 300 baud. */
#define B600			8	/**< 600 baud. */
#define B1200			9	/**< 1200 baud. */
#define B1800			10	/**< 1800 baud. */
#define B2400			11	/**< 2400 baud. */
#define B4800			12	/**< 4800 baud. */
#define B9600			13	/**< 9600 baud. */
#define B19200			14	/**< 19200 baud. */
#define B38400			15	/**< 38400 baud. */
#define B57600			16	/**< 57600 baud. */
#define B115200			17	/**< 115200 baud. */

/** Action flags for tcsetattr(). */
#define TCSANOW			1	/**< Change attributes immediately. */
#define TCSADRAIN		2	/**< Change attributes when output has drained. */
#define TCSAFLUSH		3	/**< Change attributes when output has drained; also flush pending input. */

/** Action flags for tcflush(). */
#define TCIFLUSH		0x0001	/**< Flush pending input. */
#define TCOFLUSH		0x0002	/**< Flush untransmitted output. */
#define TCIOFLUSH		0x0003	/**< Flush both pending input and untransmitted output. */

/** Action flags for tcflow(). */
#define TCIOFF			0	/**< Transmit a STOP character, intended to suspend input data. */
#define TCION			1	/**< Transmit a START character, intended to restart input data. */
#define TCOOFF			2	/**< Suspend output. */
#define TCOON			3	/**< Restart output. */

/** Terminal ioctl() requests. */
#define TIOCDRAIN		32	/**< Implements tcdrain(). */
#define TCXONC			33	/**< Implements tcflow(). */
#define TCFLSH			34	/**< Implements tcflush(). */
#define TCGETA			35	/**< Implements tcgetattr(). */
#define TCSETA			36	/**< Implements tcsetattr(fd, TCSANOW). */
#define TCSETAW			37	/**< Implements tcsetattr(fd, TCSADRAIN). */
#define TCSETAF			38	/**< Implements tcsetattr(fd, TCSAFLUSH). */
#define TIOCGPGRP		39	/**< Implements tcgetpgrp(). */
#define TIOCSPGRP		40	/**< Implements tcsetpgrp(). */
#define TIOCGWINSZ		41	/**< Get terminal size. */
#define TIOCSWINSZ		42	/**< Set terminal size. */

/** Terminal master requests. */
#define TTY_MASTER_ID		64	/**< Get the slave device ID. */

extern speed_t cfgetispeed(const struct termios *tio);
extern speed_t cfgetospeed(const struct termios *tio);
extern int cfsetispeed(struct termios *tio, speed_t speed);
extern int cfsetospeed(struct termios *tio, speed_t speed);
extern int tcdrain(int fd);
extern int tcflow(int fd, int action);
extern int tcflush(int fd, int action);
extern int tcgetattr(int fd, struct termios *tiop);
extern pid_t tcgetsid(int fd);
extern int tcsendbreak(int fd, int duration);
extern int tcsetattr(int fd, int action, const struct termios *tio);

#ifdef __cplusplus
}
#endif

#endif /* __TERMIOS_H */

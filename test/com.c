/******************************************************************************
 *
 *       Filename:  com.c
 *
 *    Description:  串口底层操作
 *
 *        Version:  1.0
 *        Created:  2020年04月14日 15时04分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  yangkun@osee-dig.com
 *        Company:  osee-dig.com
 *
 *****************************************************************************/
#include "com.h"
//#include "Types/Defs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <stdarg.h>

static int fd_com = -1;
static int g_comflag = 0;//default as general uart

typedef unsigned int DWORD;


#define DEV_COM         "/dev/ttyAMA3"
// #define DEV_COM         "/dev/ttyAMA1"

/************************************************************************
 * create com device
 *
 * ret :	= 0 success
 * 		< 0 false
 ************************************************************************/
int CommCreate(void)
{

	g_comflag = 1;

	if (g_comflag == 0)
	{
		com_print("Now the com as General UART.\n");
	}
	
	return 0;
}


int CommOpen(void)
{	
	
	if (g_comflag == 0)
		return 0;

	if (fd_com < 0)
	{
		printf("will open\n");
		com_print("dev com name:%s\n", DEV_COM);
		fd_com = open(DEV_COM, O_RDWR | O_NONBLOCK);
		if (fd_com < 0)
		{
			com_error("open com dev:%s\n", strerror(errno));
			return -1;
		}
	}
	com_print("CommOpen Successful, fd_com=%d\n", fd_com);
	return fd_com;
}



/************************************************************************
 * Close com device
 *
 * ret :	= 0 success
 * 		< 0 false
 ************************************************************************/
int CommDestory(void)
{
	int ret = -1;

	if(fd_com > 0)
	{
		ret = close(fd_com);
		fd_com = -1;		
		g_comflag = 0;//default as general uart	
	}

	return ret;
}

int CommClose(void)
{	
	//because com2com cannot be closed so return imediataly
	//ugly!
	
	return 0;
}

int SetAttribute(int dev_fd, CommAttribute *pattr)
{
	struct termios opt;
	CommAttribute *attr = pattr;
	
	if (dev_fd < 0)
	{
		return -1;
	}

	memset(&opt, 0, sizeof(struct termios));
	tcgetattr(dev_fd, &opt);
	cfmakeraw(&opt);			/* set raw mode	*/

	/*
	* set speed
	*/
	switch (attr->baudrate)
	{
		case 50:
			cfsetispeed(&opt, B50);
			cfsetospeed(&opt, B50);
			break;
		case 75:
			cfsetispeed(&opt, B75);
			cfsetospeed(&opt, B75);
			break;
		case 110:
			cfsetispeed(&opt, B110);
			cfsetospeed(&opt, B110);
			break;
		case 134:
			cfsetispeed(&opt, B134);
			cfsetospeed(&opt, B134);
			break;
		case 150:
			cfsetispeed(&opt, B150);
			cfsetospeed(&opt, B150);
			break;
		case 200:
			cfsetispeed(&opt, B200);
			cfsetospeed(&opt, B200);
			break;
		case 300:
			cfsetispeed(&opt, B300);
			cfsetospeed(&opt, B300);
			break;
		case 600:
			cfsetispeed(&opt, B600);
			cfsetospeed(&opt, B600);
			break;
		case 1200:
			cfsetispeed(&opt, B1200);
			cfsetospeed(&opt, B1200);
			break;
		case 1800:
			cfsetispeed(&opt, B1800);
			cfsetospeed(&opt, B1800);
			break;
		case 2400:
			cfsetispeed(&opt, B2400);
			cfsetospeed(&opt, B2400);
			break;
		case 4800:
			cfsetispeed(&opt, B4800);
			cfsetospeed(&opt, B4800);
			break;
		case 9600:
			cfsetispeed(&opt, B9600);
			cfsetospeed(&opt, B9600);
			break;
		case 19200:
			cfsetispeed(&opt, B19200);
			cfsetospeed(&opt, B19200);
			break;
		case 38400:
			cfsetispeed(&opt, B38400);
			cfsetospeed(&opt, B38400);
			break;
		case 57600:
			cfsetispeed(&opt, B57600);
			cfsetospeed(&opt, B57600);
			break;
		case 115200:
			cfsetispeed(&opt, B115200);
			cfsetospeed(&opt, B115200);
			break;
		case 230400:
			cfsetispeed(&opt, B230400);
			cfsetospeed(&opt, B230400);
			break;
		case 460800:
			cfsetispeed(&opt, B460800);
			cfsetospeed(&opt, B460800);
			break;
		case 500000:
			cfsetispeed(&opt, B500000);
			cfsetospeed(&opt, B500000);
			break;
		case 576000:
			cfsetispeed(&opt, B576000);
			cfsetospeed(&opt, B576000);
			break;
		case 921600:
			cfsetispeed(&opt, B921600);
			cfsetospeed(&opt, B921600);
			break;
		case 1000000:
			cfsetispeed(&opt, B1000000);
			cfsetospeed(&opt, B1000000);
			break;
		case 1152000:
			cfsetispeed(&opt, B1152000);
			cfsetospeed(&opt, B1152000);
			break;
		case 1500000:
			cfsetispeed(&opt, B1500000);
			cfsetospeed(&opt, B1500000);
			break;
		case 2000000:
			cfsetispeed(&opt, B2000000);
			cfsetospeed(&opt, B2000000);
			break;
		case 2500000:
			cfsetispeed(&opt, B2500000);
			cfsetospeed(&opt, B2500000);
			break;
		case 3000000:
			cfsetispeed(&opt, B3000000);
			cfsetospeed(&opt, B3000000);
			break;
		case 3500000:
			cfsetispeed(&opt, B3500000);
			cfsetospeed(&opt, B3500000);
			break;
		case 4000000:
			cfsetispeed(&opt, B4000000);
			cfsetospeed(&opt, B4000000);
			break;
		default:
			com_error("unsupported baudrate %d\n", attr->baudrate);
			break;
	}

	/*
	* set parity
	*/
	switch (attr->parity)
	{
		case commNoParity:		/* none			*/
			opt.c_cflag &= ~PARENB;	/* disable parity	*/
			opt.c_iflag &= ~INPCK;	/* disable parity check	*/
			break;
		case commOddParity:		/* odd			*/
			opt.c_cflag |= PARENB;	/* enable parity	*/
			opt.c_cflag |= PARODD;	/* odd			*/
			opt.c_iflag |= INPCK;	/* enable parity check	*/
			break;
		case commEvenParity:		/* even			*/
			opt.c_cflag |= PARENB;	/* enable parity	*/
			opt.c_cflag &= ~PARODD;	/* even			*/
			opt.c_iflag |= INPCK;	/* enable parity check	*/
		default:
			com_print("unsupported parity %d\n", attr->parity);
			break;
	}

	/*
	* set data bits
	*/
	opt.c_cflag &= ~CSIZE;
	switch (attr->dataBits)
	{
		case 5:
			opt.c_cflag |= CS5;
			break;
		case 6:
			opt.c_cflag |= CS6;
			break;
		case 7:
			opt.c_cflag |= CS7;
			break;
		case 8:
			opt.c_cflag |= CS8;
			break;
		default:
			com_print("unsupported data bits %d\n", attr->dataBits);
			break;
	}

	/*
	* set stop bits
	*/
	opt.c_cflag &= ~CSTOPB;
	switch (attr->stopBits)
	{
		case commOneStopBit:
			opt.c_cflag &= ~CSTOPB;
			break;
/*		case COMM_ONE5STOPBIT:
			break;
*/
		case commTwoStopBits:
			opt.c_cflag |= CSTOPB;
			break;
		default:
			com_print("unsupported stop bits %d\n", attr->stopBits);
			break;
	}
	opt.c_cc[VTIME]	= 0;
	opt.c_cc[VMIN]	= 1;			/* block until data arrive */

	tcflush(dev_fd, TCIOFLUSH);
	if (tcsetattr(dev_fd, TCSANOW, &opt) < 0)
	{
		com_error("tcsetattr : %s\n", strerror(errno));
		return -1;
	}

	return 0;
} /* end SetAttribute */

/************************************************************************
 * Set com device attribute
 *
 * ret :	= 0 success
 * 		< 0 false
 ************************************************************************/
int CommSetAttribute(CommAttribute *pattr)
{
	if(g_comflag == 0)
	{
		return 0;
	}
	
	if (fd_com < 0)
	{
		fd_com = open(DEV_COM, O_RDWR | O_NONBLOCK);
		if (fd_com < 0)
		{
			com_error("Can't Open Com Dev:%s\n", strerror(errno));
			return -1;
		}		
	}


	return SetAttribute(fd_com, pattr);
}

/************************************************************************
 * Open com device
 *
 * ret :	> 0  bytes by read
 * 		<= 0 false
 ************************************************************************/
int CommRead(void *pdata, DWORD nbytes)
{	
	printf("g_comflag = %d , fd_com=%d\n", g_comflag, fd_com);
	printf("nbytes:%d\n", nbytes);
	if(g_comflag == 0)
	{
		return 0;
	}
	
	if (fd_com < 0)
	{
		printf("will read data\n");
		fd_com = open(DEV_COM, O_RDWR | O_NONBLOCK);
		if (fd_com < 0)
		{
			com_error("Can't Open Com Dev:%s\n", strerror(errno));
			return -1;
		}		
	}
	
	return read(fd_com, pdata, nbytes);
}

/************************************************************************
 * write data
 *
 * ret :	> 0  bytes by write
 * 		<= 0 false
 ************************************************************************/
int CommWrite(void *pdata, DWORD len)
{	
	if(g_comflag == 0)
	{
		return 0;
	}

	if (fd_com < 0)
	{
		fd_com = open(DEV_COM, O_RDWR | O_NONBLOCK);
		if (fd_com < 0)
		{
			com_error("Can't Open Com Dev:%s\n", strerror(errno));
			return -1;
		}		
	}
	
	return write(fd_com, pdata, len);
}

/************************************************************************
 * stop read and write com device, or clear in/out buf
 *
 * ret :	= 0 success
 * 		< 0 false
 ************************************************************************/
int CommPurge(DWORD dw_flags)
{
	if(g_comflag == 0)
	{
		return 0;
	}

	if (fd_com < 0)
	{
		fd_com = open(DEV_COM, O_RDWR | O_NONBLOCK);
		if (fd_com < 0)
		{
			com_error("Can't Open Com Dev:%s\n", strerror(errno));
			return -1;
		}		
	}

	switch (dw_flags)
	{
		case commPurgeTxAbort:
			tcflow(fd_com, TCOOFF);
			break;
		case commPurgeRxAbort:
			tcflow(fd_com, TCIOFF);
			break;
		case commPurgeTxClear:
			tcflush(fd_com, TCOFLUSH);
			break;
		case commPurgeRxClear:
			tcflush(fd_com, TCIFLUSH);
			break;
		default:
			com_print("Unknow flag\n");
			return -1;
	}

	return 0;
}

static int g_comprint = 0;

int CommPrint(char *fmt, ...)
{
	int ret = 0;

	if (g_comprint)
	{
		va_list ap;
		
		va_start(ap, fmt);
		ret = vfprintf(stdout, fmt, ap);
		va_end(ap);
	}

	return ret;
}

int CommAsConsole(int flag)
{
	//g_comflag = !flag;
	
	return 0;
}

/*
	console = 0 use as general uart
	console = 1 use as transfer com
*/
void CommGetConsol(int *console)
{
	*console = g_comflag;
	
	return;	
	
}

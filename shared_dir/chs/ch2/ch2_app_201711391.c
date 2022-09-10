/*
 * Source for creating applications that use the device.
 * The device simply supports nonnegative integer addtion
 * and multiplication for long types.
 */
#include <stdio.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>


#define CH2_DEV_NAME		"ch2"

/* it's not registered in the ioctl-number doc and matches project name. */
#define CH2_MINOR		0xC2

#define CH2_BASIC		0x10
#define CH2_OP			0x40

typedef struct _CH2_READWRITE {
	long retval;
	long val;
//	const char *exp;
} CH2_READWRITE;

#define CH2_GET			_IO(CH2_MINOR, CH2_BASIC+1)
#define CH2_SET			_IOW(CH2_MINOR,CH2_BASIC+2,CH2_READWRITE)

#define CH2_ADD			_IOWR(CH2_MINOR,CH2_OP+1,CH2_READWRITE)
#define CH2_MUL			_IOWR(CH2_MINOR,CH2_OP+3,CH2_READWRITE)


#define CH2_FUNC(func,x,...)					\
  do {								\
    if ((x = func(__VA_ARGS__)) < 0) {				\
      perror("["CH2_DEV_NAME"_app]: " #func"() failed");	\
      return 1;							\
    }								\
  } while (0)

#define CH2_OPEN(x,...)		CH2_FUNC(open,x,__VA_ARGS__)
#define CH2_IOCTL(x,...)	CH2_FUNC(ioctl,x,__VA_ARGS__)


#define CH2_DEV_PATH 		"/dev/"CH2_DEV_NAME


#define CH2_PRINTF(...)		printf("["CH2_DEV_NAME"_app]: " __VA_ARGS__)

#ifdef LOCAL
#define eprintf(...)		fprintf(stderr, __VA_ARGS__), fflush(stderr)
#else
#define eprintf(...)		(void) 0
#endif


int main(int argc, char *argv[])
{
	int dev;
	long res;
	CH2_READWRITE data;

	CH2_OPEN(dev, CH2_DEV_PATH, O_RDWR | O_NONBLOCK); /* not sure, but follow what the man page ioctl NOTES section says. */


	data.val = 200;
	CH2_IOCTL(res, dev, CH2_SET, &data);

	CH2_IOCTL(res, dev, CH2_GET, NULL);
	CH2_PRINTF("%ld\n", res);

	data.val = 300;
	CH2_IOCTL(res, dev, CH2_ADD, &data);
	CH2_PRINTF("%ld\n", data.retval);

	data.val = 100;
	CH2_IOCTL(res, dev, CH2_MUL, &data);
	CH2_PRINTF("%ld\n", data.retval);


	close(dev);

	return 0;
}

/* you should actually read the stuff from the bottom. */

/* <basic strategy>
	* the address value is passed as an argument for versatility. (ex. ioctl)
*/

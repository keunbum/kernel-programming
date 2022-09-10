/*
 * Source for creating applications that use the device.
 * A queue device that stores a string type is used.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

#include "ch3.h"

#define CH3_DEV_NAME			"ch3_dev2"
#define CH3_DEV_PATH 			"/dev/"CH3_DEV_NAME

#define CH3_FUNC(func,x,...)					\
  do {								\
    if ((x = func(__VA_ARGS__)) < 0) {				\
      perror("["CH3_DEV_NAME"_app]: " #func"() failed");	\
      return 1;							\
    }								\
  } while (0)

#define CH3_OPEN(x,...)			CH3_FUNC(open,x,__VA_ARGS__)
#define CH3_IOCTL(x,...)		CH3_FUNC(ioctl,x,__VA_ARGS__)

#define CH3_FPRINTF(ostream,...)	fprintf(ostream,"["CH3_DEV_NAME"_app]: " __VA_ARGS__), fflush(ostream)
#define CH3_PRINTF(...)			CH3_FPRINTF(stdout,__VA_ARGS__)
#define CH3_EPRINTF(...)		CH3_FPRINTF(stderr,__VA_ARGS__)

typedef struct msg_st msg_st;

#define CH3_MSG_SIZE 128

int main(int argc, char *argv[])
{
	int dev, res;
	msg_st data;

	CH3_OPEN(dev, CH3_DEV_PATH, O_RDWR); /* not sure, but follow what the man page ioctl NOTES section says. */

	char user_cmd[10];
	while (scanf("%s", user_cmd) != EOF) {
		printf("%s ", argv[0]);

		if (!strcmp(user_cmd, "write")) {
			scanf("%s\n", data.str);
			CH3_PRINTF("write %s\n", data.str);
			data.len = strlen(data.str);
			assert(data.len >= 0 && data.len < CH3_MSG_SIZE);
			CH3_IOCTL(res, dev, CH3_IOCTL_WRITE, &data);
		} else

		if (!strcmp(user_cmd, "read")) {
			CH3_IOCTL(res, dev, CH3_IOCTL_READ, &data);
			CH3_PRINTF("read %s\n", data.str);
		} else

		if (!strcmp(user_cmd, "quit") || !strcmp(user_cmd, "exit")) {
			break;
		}

		else {
			CH3_PRINTF("No CMD in list\n");
			continue;
		}

		sleep(1);
	}

	close(dev);

	return 0;
}

/* you actually read the stuff from the bottom. */

/* <basic strategy>
	* the address value is passed as an argument for versatility. (ex. ioctl)
*/

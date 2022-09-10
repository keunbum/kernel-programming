#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "../ku_c.h"

int ku_s_open(int);
int ku_s_close();
int ku_s_read(void *, int, int);
int ku_a_write(void *, int);

/* ------------------------------------------------------------------------------------*/

#define ku_func(func,x,...)					\
  do {								\
    if ((x = func(__VA_ARGS__)) < 0) {				\
      perror("ku_app: " #func"() failed");			\
      return 1;							\
    }								\
  } while (0)

#define s_open(x, ...)				ku_func(ku_s_open, x, ##__VA_ARGS__)
#define s_close(x, ...)				ku_func(ku_s_close, x, ##__VA_ARGS__)
#define s_read(x, ...)				ku_func(ku_s_read, x, ##__VA_ARGS__)
#define a_write(x, ...)				ku_func(ku_a_write, x, ##__VA_ARGS__)
#define ku_printf(fmt,...)			printf("ku_app: "fmt, ##__VA_ARGS__)

#define SING					90

int main(int argc, char *argv[])
{
	int ret;
	struct msg_buf msg;
	int len = 128;
	ku_printf("process%i\n", getpid());
	s_open(ret, KU_CREAT);
	do {
		s_read(ret, &msg, len, 0);
		ku_printf("(%dcm, time:%s[%d])\n", msg.distance, msg.timestamp, len);
		if (msg.distance < SING) {
			a_write(ret, &msg, len);
		}
		usleep(100 * 1000);
	} while (1);
	s_close(ret);
	return 0;
}

/* read stuff as if you were wrong once. --> "why is this wrong??"

 * basic strategy:
 	* understand what the problem asks for
	* simplify. a step by step approach
  	* readability is important
	* keep calm
	* do smth instead of nothing and stay organized
	* DON'T GET STUCK ON ONE APPROACH

 * stuff you should look for
  	* special cases
  	* error return handling
	* memory leak
  	* invalid argument passing
	* invalid pointer operation
	* invalid lock/unlock
	* invalid blocking handling
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum Value {
	READ,
	WRITE,
};

int main() {
	const int CNT = 10;
	int i, cmd;
	srand((unsigned int) time(NULL));
	for (i = 0; i < CNT; i++) {
		cmd = rand() % 2;
		if (cmd == 0) {
			printf("read\n");
		} else {
			printf("write hello%d%ld\n", i, clock());
		}
	}
	printf("quit");
	return 0;
}

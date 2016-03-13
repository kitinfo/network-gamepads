/*
 * copied from /cbdevnet/kbserver
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include "sockfd.c"

int main(int argc, char** argv){
	char* input_device=NULL;
	int fd, bytes, sock_fd;
	struct input_event ev;

	if(argc<2){
		printf("Insufficient arguments\n");
		exit(1);
	}

	input_device=argv[1];

	fd=open(input_device, O_RDONLY);

	if(fd<0){
		printf("Failed to open device\n");
		return 1;
	}

	sock_fd = sock_open("localhost", 7999);

	if (sock_fd < 0) {
		return 2;
	}

	//get exclusive control
	bytes=ioctl(fd, EVIOCGRAB, 1);

	while(true){
		bytes=read(fd, &ev, sizeof(struct input_event));
		if(bytes<0){
			printf("read() error\n");
			close(fd);
			return 3;
		}
		if(ev.type==EV_KEY || ev.type == EV_SYN){
			printf("type: %d, code: %d, value: %d\n", ev.type, ev.code, ev.value);
			send(sock_fd, &ev, sizeof(struct input_event), 0);
		}
	}

	return 0;
}

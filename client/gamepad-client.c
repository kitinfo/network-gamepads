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
#include <errno.h>

#include "../network.h"
#include "../protocol.h"

const unsigned MSG_MAX = 256;
const unsigned TOKEN_LEN = 64;

int continue_connect(int sock_fd, char* token) {
	ssize_t bytes = 0;
	char msg[MSG_MAX + 1];

	bytes = snprintf(msg, MSG_MAX, "CONTINUE %s %s\n", PROTOCOL_VERSION, token);
	if(bytes >= MSG_MAX) {
		printf("Message would have been too long\n");
		return -1;
	}

	send(sock_fd, msg, bytes + 1, 0);
	memset(msg, 0, MSG_MAX + 1);

	bytes = recv(sock_fd, msg, MSG_MAX, 0);
	if(bytes < 0) {
		perror("continue_connect/recv");
		return -1;
	}

	if (!strncmp(msg, "403", 3)) {
		printf("Token expired");
		return 0;
	} else if (strncmp(msg, "200", 3)) {
		printf("Unkown error (%s)", msg + 4);
		return -1;
	}
	return 1;
}

char* init_connect(int sock_fd, char* password) {
	ssize_t bytes;
	char msg[MSG_MAX + 1];

	bytes = snprintf(msg, MSG_MAX, "HELLO %s %s", PROTOCOL_VERSION, password);
	if(bytes >= MSG_MAX) {
		printf("Generated message would have been too long\n");
		return NULL;
	}

	send(sock_fd, msg, bytes + 1, 0);

	memset(msg, 0, sizeof(msg));
	bytes = recv(sock_fd, msg, MSG_MAX, 0);

	if (bytes < 0) {
		perror("init_connect/recv");
		return NULL;
	}

	if (!strncmp(msg, "401", 3)) {
		printf("Invalid password supplied\n");
		return NULL;
	} else if (!strncmp(msg, "402", 3)) {
		char* s_version = msg + 4;
		printf("Version not matched (Server: %s != Client %s)\n", s_version, PROTOCOL_VERSION);
		return NULL;
	} else if (strncmp(msg, "200", 3)) {
		char* error = msg + 4;
		printf("Unkown error (%s)\n", error);
		return NULL;
	}

	unsigned token_len = strlen(msg + 4) + 1;
	char* token = malloc(token_len);
	strncpy(token, msg + 4, token_len);

	return token;
}

int main(int argc, char** argv){
	char* host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST;
	char* port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT;
	char* password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD;

	int event_fd, sock_fd;
	ssize_t bytes;
	struct input_event ev;

	if(argc < 2){
		printf("Insufficient arguments\n");
		return EXIT_FAILURE;
	}

	printf("Reading input events from %s\n", argv[1]);
	event_fd = open(argv[1], O_RDONLY);
	if(event_fd < 0){
		printf("Failed to open device\n");
		return EXIT_FAILURE;
	}

	sock_fd = tcp_connect(host, port);
	if(sock_fd < 0) {
		printf("Failed to reach server at %s port %s\n", host, port);
		return 2;
	}

	char* token = init_connect(sock_fd, password);
	if (token == NULL) {
		return 3;
	}

	//get exclusive control
 	bytes = ioctl(event_fd, EVIOCGRAB, 1);

	while(true){
		//block on read
		bytes = read(event_fd, &ev, sizeof(ev));
		if(bytes < 0){
			printf("read() error\n");
			break;
		}
		if(bytes == sizeof(ev) && 
				(ev.type == EV_KEY || ev.type == EV_SYN || ev.type == EV_REL || ev.type == EV_ABS)){
			printf("Event type:%d, code:%d, value:%d\n", ev.type, ev.code, ev.value);
			bytes = send(sock_fd, &ev, sizeof(struct input_event), 0);

			if(bytes < 0){
				//check if connection is closed
				if(errno == ECONNRESET) {
					int status = continue_connect(sock_fd, token);
					if (status == 0) { // reconnect is failed, trying init_connect
						free(token);
						token = init_connect(sock_fd, password);
						if (token == NULL) { // cannot reconnect to server
							printf("Cannot reconnect to server.\n");
							break;
						}
					} else if (status < 0) {
						break;
					}
				} else {
					printf("read() error\n");
					break;
				}
			}
		}
		else{
			fprintf(stderr, "Short read from event descriptor (%zd bytes)\n", bytes);
		}
	}

	close(event_fd);
	close(sock_fd);

	return 0;
}

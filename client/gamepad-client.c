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

#include "sockfd.c"

const char* version = "1.0";

const unsigned MSG_MAX = 256;
const unsigned TOKEN_LEN = 64;

int continue_connect(int sock_fd, char* token) {

	unsigned token_len = strlen(token);

	if (token_len > TOKEN_LEN) {
		printf("Token too long.\n");
		return -1;
	}

	char* cont = "CONTINUE";

	unsigned msg_len = strlen(cont) + 1 + strlen(version) + 1 + token_len + 1;

	char msg[MSG_MAX + 1];

	if (msg_len > MSG_MAX) {
		printf("Message is too long.\n");
		return -1;
	}

	snprintf(msg, msg_len + 1, "%s %s %s\n", cont, version, token);

	int bytes = 0;
	if (sock_send(sock_fd, msg) < 0) {
		perror("continue_connect/send");
	}
	memset(msg, 0, MSG_MAX + 1);

	bytes = recv(sock_fd, msg, MSG_MAX, 0);

	if (bytes < 0) {
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

	unsigned pw_len = strlen(password);

	if (pw_len > 64) {
		printf("Password is too long (limit is 64 characters).\n");
		return NULL;
	}

	char* hello = "HELLO";

	unsigned msg_len = strlen(hello) + 1 + strlen(version) + 1 + pw_len;

	if (msg_len > MSG_MAX) {
		printf("Message too long.\n");
		return NULL;
	}

	char msg[MSG_MAX + 1];
	memset(msg, 0, MSG_MAX +1);
	snprintf(msg, msg_len + 1, "%s %s %s", hello, version, password);
	int bytes = 0;
	if (sock_send(sock_fd, msg) < 0) {
			perror("init_connect/send");
			return NULL;
	}
	memset(msg, 0, msg_len + 1);
	bytes = recv(sock_fd, msg, MSG_MAX, 0);

	if (bytes < 0) {
		perror("init_connect/recv");
		return NULL;
	}

	if (!strncmp(msg, "401", 3)) {
		printf("Password is wrong.\n");
		return NULL;
	} else if (!strncmp(msg, "402", 3)) {
		char* s_version = msg + 4;
		printf("Version not matched (Server: %s != Client %s).\n", s_version, version);
		return NULL;
	} else if (strncmp(msg, "200", 3)) {
		char* error = msg + 4;
		printf("Unkown error(%s)\n", error);
		return NULL;
	}

	unsigned token_len = strlen(msg + 4) + 1;
	char* token = malloc(token_len);
	strncpy(token, msg + 4, token_len);

	return token;
}

int main(int argc, char** argv){

	char* host = getenv("GAMEPAD_SERVER_HOST");
	char* port = getenv("GAMEPAD_SERVER_PORT");
	char* password = getenv("GAMEPAD_SERVER_PW");
	if (host == NULL) {
		host = "129.13.215.26";
	}

	if (port == NULL) {
		port = "7888";
	}

	if (password == NULL) {
		password = "0000";
	}

	unsigned port_num = strtoul(port, NULL, 10);

	char* input_device=NULL;
	int fd, bytes, sock_fd;
	struct input_event ev;

	if(argc<2){
		printf("Insufficient arguments\n");
		exit(1);
	}

	input_device=argv[1];

	printf("input_device: %s\n", input_device);
	fd=open(input_device, O_RDONLY);
	if(fd<0){
		printf("Failed to open device\n");
		return 1;
	}

	if (!isatty(fileno(stdout))) {
		setbuf(stdout, NULL);
	}

	sock_fd = sock_open(host, port_num);

	if (sock_fd < 0) {
		printf("Cannot connect to server (%s:%d).\n", host, port_num);
		return 2;
	}

	char* token = init_connect(sock_fd, password);

	if (token == NULL) {
		return 3;
	}

	fd_set rdfs;

	FD_ZERO(&rdfs);
	FD_SET(fd, &rdfs);

	//get exclusive control
 	bytes=ioctl(fd, EVIOCGRAB, 1);

	while(true){
		printf("preselect\n");
		select(fd + 1, &rdfs, NULL, NULL, NULL);
		printf("select\n");
		bytes = read(fd, &ev, sizeof(ev));
		printf("event\n");
		if(bytes < 0){
			printf("read() error\n");
			break;
		}
		printf("type: %d, code: %d, value: %d\n", ev.type, ev.code, ev.value);
		if(ev.type==EV_KEY || ev.type == EV_SYN || ev.type == EV_REL || ev.type == EV_ABS){
			bytes = send(sock_fd, &ev, sizeof(struct input_event), 0);

			if(bytes<0){
				// check if connection is closed
				if (errno == ECONNRESET) {
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
	}
	close(fd);
	close(sock_fd);

	return 0;
}

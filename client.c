#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <unistd.h>
#include <errno.h>

#include "libs/logger.h"
#include "libs/logger.c"
#include "libs/easy_args.h"
#include "libs/easy_args.c"

#include "network.h"
#include "protocol.h"
#include "client.h"

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

char* init_connect(int sock_fd, int device_fd, Config* config) {
	ssize_t bytes;
	//FIXME this only allocates enough storage for one message
	char msg[MSG_MAX + 1];

	struct input_id id = {0};

	if (ioctl(device_fd, EVIOCGID, &id) < 0) {
		logprintf(config->log, LOG_ERROR, "Cannot query device infos: %s\n", strerror(errno));
		return NULL;
	}
	char dev_name[UINPUT_MAX_NAME_SIZE];
	memset(dev_name, 0, sizeof(dev_name));
	if (ioctl(device_fd, EVIOCGNAME(sizeof(dev_name) - 1), &dev_name) < 0) {
		logprintf(config->log, LOG_ERROR, "Cannot query device name: %s\n", strerror(errno));
		return NULL;
	}

	bytes = snprintf(msg, MSG_MAX, "HELLO %s\nVENDOR 0x%.4x\nPRODUCT 0x%.4x\nBUSTYPE 0x%.4x\nDEVTYPE %d\nVERSION 0x%.4x\nNAME %s\nPASSWORD %s\n\n", PROTOCOL_VERSION, id.vendor, id.product, id.bustype, config->type, id.version, dev_name, config->password);
	logprintf(config->log, LOG_DEBUG, "Generated message: %s", msg);
	if(bytes >= MSG_MAX) {
		logprintf(config->log, LOG_ERROR, "Generated message would have been too long\n");
		return NULL;
	}

	send(sock_fd, msg, bytes + 1, 0);

	memset(msg, 0, sizeof(msg));
	//FIXME this might not be in one message
	bytes = recv(sock_fd, msg, MSG_MAX, 0);

	if (bytes < 0) {
		logprintf(config->log, LOG_ERROR, "init_connect/recv: %s\n", strerror(errno));
		return NULL;
	}
	logprintf(config->log, LOG_DEBUG, "Answer: %s\n", msg);
	if (!strncmp(msg, "401", 3)) {
		logprintf(config->log, LOG_ERROR, "Invalid password supplied\n");
		return NULL;
	} else if (!strncmp(msg, "400", 3)) {
		char* s_version = msg + 4;
		logprintf(config->log, LOG_ERROR, "Version not matched (Server: %s != Client %s)\n", s_version, PROTOCOL_VERSION);
		return NULL;
	} else if (strncmp(msg, "200", 3)) {
		char* error = msg + 4;
		logprintf(config->log, LOG_ERROR, "Unkown error (%s)\n", error);
		return NULL;
	}

	unsigned token_len = strlen(msg + 4) + 1;
	char* token = malloc(token_len);
	strncpy(token, msg + 4, token_len);

	return token;
}

int setType(int argc, char** argv, Config* config) {
	if (!strcmp(argv[1], "mouse")) {
		config->type = 1;
	} else if (!strcmp(argv[1], "gamepad")) {
		config->type = 2;
	} else if (!strcmp(argv[1], "keyboard")) {
		config->type = 3;
	} else {
		return -1;
	}

	return 0;
}

void add_arguments(Config* config) {
	eargs_addArgument("-t", "--type", setType, 1);
	eargs_addArgumentUInt("-v", "--verbosity", &config->log.verbosity);
}

int main(int argc, char** argv){

	Config config = {
		.log = {
			.stream = stderr,
			.verbosity = 5
		},
		.program_name = argv[0],
		.host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.type = 0
	};

	int event_fd, sock_fd;
	ssize_t bytes;
	struct input_event ev;

	add_arguments(&config);
	char* output[argc];
	int outputc = eargs_parse(argc, argv, output, &config);

	if(outputc < 1){
		logprintf(config.log, LOG_ERROR, "Insufficient arguments\n");
		return EXIT_FAILURE;
	}

	logprintf(config.log, LOG_INFO, "Reading input events from %s\n", output[0]);
	event_fd = open(output[0], O_RDONLY);
	if(event_fd < 0){
		logprintf(config.log, LOG_ERROR, "Failed to open device\n");
		return EXIT_FAILURE;
	}

	sock_fd = tcp_connect(config.host, config.port);
	if(sock_fd < 0) {
		logprintf(config.log, LOG_ERROR, "Failed to reach server at %s port %s\n", config.host, config.port);
		return 2;
	}

	char* token = init_connect(sock_fd, event_fd, &config);
	if (token == NULL) {
		return 3;
	}
	logprintf(config.log, LOG_INFO, "token %s\n", token);
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
				(ev.type == EV_KEY || ev.type == EV_SYN || ev.type == EV_REL || ev.type == EV_ABS || ev.type == EV_MSC)){
			logprintf(config.log, LOG_DEBUG, "Event type:%d, code:%d, value:%d\n", ev.type, ev.code, ev.value);
			bytes = send(sock_fd, &ev, sizeof(struct input_event), 0);

			if(bytes < 0){
				//check if connection is closed
				if(errno == ECONNRESET) {
					int status = continue_connect(sock_fd, token);
					if (status == 0) { // reconnect is failed, trying init_connect
						free(token);
						token = init_connect(sock_fd, event_fd, &config);
						if (token == NULL) { // cannot reconnect to server
							logprintf(config.log, LOG_ERROR, "Cannot reconnect to server.\n");
							break;
						}
					} else if (status < 0) {
						break;
					}
				} else {
					logprintf(config.log, LOG_ERROR, "read() error\n");
					break;
				}
			}
		}
		else{
			if (bytes == sizeof(ev)) {
				logprintf(config.log, LOG_WARNING, "Unsupported event type (type = %d)\n", ev.type);
			} else {
				logprintf(config.log, LOG_WARNING, "Short read from event descriptor (%zd bytes)\n", bytes);
			}
		}
	}
	close(event_fd);
	close(sock_fd);

	return 0;
}

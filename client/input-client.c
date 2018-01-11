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
#include <signal.h>
#include <dirent.h>
#include <limits.h>

#if __BSD_SOURCE
#include <sys/endian.h>
#else

#include <endian.h>
#endif

#include "../libs/logger.h"
#include "../libs/easy_args.h"

#include "../common/network.h"
#include "../common/protocol.h"
#include "input-client.h"

#define INPUT_NODES "/dev/input"
#define EVENT_PREFIX "event"

sig_atomic_t quit_signal = false;

bool get_abs_info(Config* config, int device_fd, int abs, struct input_absinfo* info) {
	if (ioctl(device_fd, EVIOCGABS(abs), info)) {
		logprintf(config->log, LOG_INFO, "Failed to find absolute axis %d\n", abs);
		return false;
	}
	logprintf(config->log, LOG_DEBUG, "Absolute axis %d: range %d - %d, value %d, fuzz %d, flat %d\n", abs, info->minimum, info->maximum, info->value, info->fuzz, info->flat);
	return true;
}

bool send_key_info(int sock_fd, int device_fd, Config* config) {

	unsigned long types[EV_MAX];
	unsigned long keys[(KEY_MAX - 1) / (sizeof(unsigned long) * 8) + 1];

	RequestEventMessage msg = {0};
	msg.msg_type = MESSAGE_REQUEST_EVENT;

	memset(&types, 0, sizeof(types));

	int t_bytes = ioctl(device_fd, EVIOCGBIT(0, EV_MAX), types);
	if (t_bytes <= 0) {
		logprintf(config->log, LOG_ERROR, "Error getting EV types: %s.\n", strerror(errno));
		return 0;
	}

	int i, j;
	int k_bytes;
	ABSInfoMessage abs_msg = {
		.msg_type = MESSAGE_ABSINFO
	};
	for (i = 0; i < EV_MAX; i++) {
		if ((types[i / (sizeof(unsigned long) * 8)] & ((unsigned long) 1 << i % (sizeof(unsigned long) * 8))) > 0) {
			memset(&keys, 0, sizeof(keys));

			k_bytes = ioctl(device_fd, EVIOCGBIT(i, sizeof(keys)), keys) ;
			if (k_bytes < 0) {
				logprintf(config->log, LOG_ERROR, "Error getting %d type bits: %s.\n", i, strerror(errno));
				return false;
			}

			for (j = 0; j < k_bytes * 8; j++) {
				if ((keys[j / (sizeof(unsigned long) * 8)] & ((unsigned long) 1 << j % (sizeof(unsigned long) * 8))) > 0) {
					logprintf(config->log, LOG_DEBUG, "%d:%d bit enabled\n", i, j);
					msg.type = i;
					msg.code = j;

					if (!send_message(config->log, sock_fd, &msg, sizeof(msg))) {
						return false;
					}

					if (i == EV_ABS) {
						memset(&abs_msg.info, 0, sizeof(abs_msg.info));
						if (!get_abs_info(config, device_fd, j, &abs_msg.info)) {
							return false;
						}
						abs_msg.axis = j;
						if (!send_message(config->log, sock_fd, &abs_msg, sizeof(abs_msg))) {
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

bool setup_device(int sock_fd, int device_fd, Config* config) {
	ssize_t msglen = sizeof(DeviceMessage) + UINPUT_MAX_NAME_SIZE;
	DeviceMessage* msg = malloc(msglen);
	memset(msg, 0, msglen);

	msg->msg_type = MESSAGE_DEVICE;
	msg->length = UINPUT_MAX_NAME_SIZE;

	if (ioctl(device_fd, EVIOCGID, &(msg->id)) < 0) {
		logprintf(config->log, LOG_ERROR, "Failed to query device ID: %s\n", strerror(errno));
		free(msg);
		return false;
	}

	if (config->dev_name) {
		strncpy(msg->name, config->dev_name, UINPUT_MAX_NAME_SIZE);
		msg->name[UINPUT_MAX_NAME_SIZE - 1] = 0;
	} else {
		if (ioctl(device_fd, EVIOCGNAME(UINPUT_MAX_NAME_SIZE - 1), msg->name) < 0) {
			logprintf(config->log, LOG_ERROR, "Failed to query device name: %s\n", strerror(errno));
			free(msg);
			return false;
		}
	}

	logprintf(config->log, LOG_DEBUG, "Starting device setup\n");
	if (!send_message(config->log, sock_fd, msg, msglen)) {
		free(msg);
		return false;
	}

	free(msg);

	if (!send_key_info(sock_fd, device_fd, config)) {
		return false;
	}

	uint8_t msg_type = MESSAGE_SETUP_END;
	if (!send_message(config->log, sock_fd, &msg_type, 1)) {
		return false;
	}

	return true;
}

bool init_connect(int sock_fd, int device_fd, Config* config) {
	logprintf(config->log, LOG_INFO, "Connecting...\n");

	uint8_t buf[INPUT_BUFFER_SIZE];
	ssize_t recv_bytes;

	HelloMessage hello = {
		.msg_type = MESSAGE_HELLO,
		.version = PROTOCOL_VERSION,
		.slot = config->slot
	};

	// send hello message
	if (!send_message(config->log, sock_fd, &hello, sizeof(hello))) {
		return false;
	}

	recv_bytes = recv_message(config->log, sock_fd, buf, sizeof(buf), NULL, 0);
	if (recv_bytes < 0) {
		return false;
	}

	logprintf(config->log, LOG_DEBUG, "Received message type: %s\n", get_message_name(buf[0]));

	// check version
	if (buf[0] == MESSAGE_VERSION_MISMATCH) {
		logprintf(config->log, LOG_ERROR, "Version mismatch: %.2x (client) incompatible to %.2x (server)\n", PROTOCOL_VERSION, buf[1]);
		return false;
	} else if (buf[0] == MESSAGE_PASSWORD_REQUIRED) {
		logprintf(config->log, LOG_INFO, "Exchanging authentication...\n");
		int pwlen = strlen(config->password) + 1;

		if (pwlen > 254) {
			logprintf(config->log, LOG_ERROR, "Password is too long.\n");
			return false;
		}
		// msg_type byte + length byte + pwlen
		PasswordMessage* passwordMessage = calloc(2 + strlen(config->password) + 1, sizeof(char));

		if (!passwordMessage) {
			logprintf(config->log, LOG_ERROR, "Cannot allocate memory.\n");
			return false;
		}

		passwordMessage->msg_type = MESSAGE_PASSWORD;
		passwordMessage->length = strlen(config->password) + 1;
		strcpy((char*)&(passwordMessage->password), config->password);

		if (!send_message(config->log, sock_fd, passwordMessage, 2 + strlen(config->password) + 1)) {
			free(passwordMessage);
			return false;
		}
		free(passwordMessage);

		recv_bytes = recv_message(config->log, sock_fd, buf, sizeof(buf), NULL, 0);
		if (recv_bytes < 0) {
			return false;
		}
		logprintf(config->log, LOG_DEBUG, "Received message type: 0x%.2x\n", buf[0]);
	}

	if (buf[0] == MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_INFO, "Setup requested by server\n");
		if (!setup_device(sock_fd, device_fd, config)) {
			return false;
		}

		recv_bytes = recv_message(config->log, sock_fd, buf, sizeof(buf), NULL, 0);
		if (recv_bytes < 0) {
			return false;
		}
	}

	if (buf[0] != MESSAGE_SUCCESS) {
		logprintf(config->log, LOG_ERROR, "Connection failed, last message: %.2x\n", buf[0]);
		return false;
	}

	logprintf(config->log, LOG_INFO, "Connected to slot %d\n", buf[1]);
	printf("Ready...\n");
	config->slot = buf[1];

	return true;
}

void quit() {
	quit_signal = true;
}

int usage(int argc, char** argv, Config* config) {
	printf("%s usage:\n"
			"%s [<options>] <device>\n"
			"    -c, --continue <slot>   - Request connection continuation on a given slot (1-255)\n"
			"    -h, --host <host>       - Specify host to connect to\n"
			"    -?, --help              - Display this help text\n"
			"    -r,--reopen <x>         - Try to reopen device for x seconds after it disconnects (-1 retries indefinitely)\n"
			"    -p, --port              - Specify InputServer port\n"
			"    -n, --name              - Specify a name for mapping on the server\n"
			"    -pw,--password <pw>     - Set a connection password\n"
			"    -v, --verbosity <level> - Debug verbosity (0: ERROR to 5: DEBUG)\n"
			,config->program_name, config->program_name);
	return -1;
}

int set_slot(int argc, char** argv, Config* config) {
	unsigned value = strtoul(argv[1], NULL, 10);

	if (value > 255 || value == 0) {
		logprintf(config->log, LOG_ERROR, "Connection slot range exceeded, required to be in range 1-255\n");
		return -1;
	}

	logprintf(config->log, LOG_INFO, "Selected connection slot %d\n");
	config->slot = value;

	return 1;
}

void add_arguments(Config* config) {
	eargs_addArgument("-?", "--help", usage, 0);
	eargs_addArgumentString("-h", "--host", &config->host);
	eargs_addArgumentString("-n", "--name", &config->dev_name);
	eargs_addArgumentString("-p", "--port", &config->port);
	eargs_addArgumentString("-pw", "--password", &config->password);
	eargs_addArgumentUInt("-v", "--verbosity", &config->log.verbosity);
	eargs_addArgument("-c", "--continue", set_slot, 1);
	eargs_addArgumentInt("-r", "--reopen", &config->reopen_attempts);
}

int device_reopen(Config* config, char* file) {
	int fd = -1;

	int counter = config->reopen_attempts;

	while (!quit_signal && counter != 0) {
		fd = open(file, O_RDONLY);
		if (fd >= 0) {
			//get exclusive control
			int grab = 1;
 			if (ioctl(fd, EVIOCGRAB, &grab) < 0) {
				logprintf(config->log, LOG_WARNING, "Failed to request exclusive access to device: %s\n", strerror(errno));
				close(fd);
				return -1;
			}
			return fd;
		}
		logprintf(config->log, LOG_ERROR, "Failed to reconnect, waiting...\n");
		sleep(1);

		counter--;
	}

	logprintf(config->log, LOG_ERROR, "User signal, terminating\n");
	return -1;
}

int scan_devices(Config* config) {
	int fd = -1;
	size_t u;
	struct dirent* file = NULL;
	char file_path[PATH_MAX * 2];
	char device_name[UINPUT_MAX_NAME_SIZE];
	DIR* input_files = opendir(INPUT_NODES);
	if(!input_files){
		logprintf(config->log, LOG_ERROR, "Failed to query input device nodes: %s\n", strerror(errno));
		return 1;
	}

	for(file = readdir(input_files); file; file = readdir(input_files)){
		if(!strncmp(file->d_name, EVENT_PREFIX, strlen(EVENT_PREFIX)) && file->d_type == DT_CHR){
			snprintf(file_path, sizeof(file_path), "%s/%s", INPUT_NODES, file->d_name);

			fd = open(file_path, O_RDONLY);
			if(fd < 0){
				logprintf(config->log, LOG_WARNING, "Problematic device %s: %s\n", file_path, strerror(errno));
				continue;
			}

			if(ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0){
				logprintf(config->log, LOG_WARNING, "Failed to read name of %s: %s\n", file_path, strerror(errno));
				close(fd);
				continue;
			}

			printf("\t%s) %s (%s)\n", file->d_name + strlen(EVENT_PREFIX), device_name, file_path);
			close(fd);
		}
	}

	if(!fgets(device_name, sizeof(device_name), stdin)){
		logprintf(config->log, LOG_ERROR, "Failed to read device identifier\n");
		closedir(input_files);
		return 1;
	}

	//trim input
	for(u = 0; u < strlen(device_name); u++){
		if(!isprint(device_name[u])){
			device_name[u] = 0;
			break;
		}
	}

	snprintf(file_path, sizeof(file_path), "%s/%s%s", INPUT_NODES, EVENT_PREFIX, device_name);
	config->dev_path = strdup(file_path);
	closedir(input_files);
	return 0;
}



int run(Config* config, int event_fd) {
	struct input_event event;
	int sock_fd;
	ssize_t bytes;
	struct sigaction act = {
		.sa_handler = &quit
	};
	DataMessage data = {0};
	data.msg_type = MESSAGE_DATA;

	if (sigaction(SIGINT, &act, NULL) < 0) {
		logprintf(config->log, LOG_ERROR, "Failed to set signal mask\n");
		return 10;
	}

	sock_fd = tcp_connect(config->host, config->port);
	if(sock_fd < 0) {
		logprintf(config->log, LOG_ERROR, "Failed to reach server at %s port %s\n", config->host, config->port);
		return 2;
	}

	if (!init_connect(sock_fd, event_fd, config)) {
		close(sock_fd);
		return 3;
	}

	//get exclusive control
	int grab = 1;
 	if (ioctl(event_fd, EVIOCGRAB, &grab) < 0) {
		logprintf(config->log, LOG_WARNING, "Failed to request exclusive access to device: %s\n", strerror(errno));
		close(sock_fd);
		return 4;
	}

	while(!quit_signal){
		//block on read
		bytes = read(event_fd, &event, sizeof(event));
		if(bytes < 0) {
			logprintf(config->log, LOG_ERROR, "read() failed: %s\nReconnecting...\n", strerror(errno));
			close(event_fd);
			event_fd = device_reopen(config, config->dev_path);
			if (event_fd < 0) {
				break;
			} else {
				continue;
			}
		}
		if(bytes == sizeof(event)) {
			logprintf(config->log, LOG_DEBUG, "Event type:%d, code:%d, value:%d\n", event.type, event.code, event.value);

			data.type = htobe16(event.type);
			data.code = htobe16(event.code);
			data.value = htobe32(event.value);

			if(!send_message(config->log, sock_fd, &data, sizeof(data))) {
				//check if connection is closed
				if(errno == ECONNRESET || errno == EPIPE) {
					if (!init_connect(sock_fd, event_fd, config)) {
						logprintf(config->log, LOG_ERROR, "Reconnection failed: %s\n", strerror(errno));
						break;
					}
				} else {
					logprintf(config->log, LOG_ERROR, "Failed to send: %s\n", strerror(errno));
					break;
				}
			}
		} else{
			logprintf(config->log, LOG_WARNING, "Short read from event descriptor (%zd bytes)\n", bytes);
		}
	}
	if (sock_fd != -1) {
		uint8_t quit_msg = MESSAGE_QUIT;
		send_message(config->log, sock_fd, &quit_msg, sizeof(quit_msg));
		close(sock_fd);
	}

	return 0;
}

int main(int argc, char** argv){
	Config config = {
		.log = {
			.stream = stderr,
			.verbosity = 0
		},
		.program_name = argv[0],
		.dev_name = NULL,
		.dev_path = NULL,
		.host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.type = 0,
		.slot = 0
	};

	int event_fd;
	char* output[argc];

	add_arguments(&config);
	int outputc = eargs_parse(argc, argv, output, &config);

	printf("%s, starting up\n", VERSION);

	if(outputc < 1){
		logprintf(config.log, LOG_ERROR, "Please select an input device:\n");
		if (scan_devices(&config)) {
			logprintf(config.log, LOG_ERROR, "Failed to open input device\n");
			return EXIT_FAILURE;
		}
	} else {
		config.dev_path = strdup(output[0]);
	}

	logprintf(config.log, LOG_INFO, "Reading input events from %s\n", config.dev_path);
	event_fd = open(config.dev_path, O_RDONLY);
	if(event_fd < 0){
		logprintf(config.log, LOG_ERROR, "Failed to open device %s: %s\n", config.dev_path, strerror(errno));
		free(config.dev_path);
		return EXIT_FAILURE;
	}

	printf("Connection negotiated, now streaming\n");
	int status = run(&config, event_fd);
	close(event_fd);
	free(config.dev_path);
	return status;
}

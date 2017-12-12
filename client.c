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

#include "libs/logger.h"
#include "libs/logger.c"
#include "libs/easy_args.h"
#include "libs/easy_args.c"

#include "network.h"
#include "protocol.h"
#include "client.h"

sig_atomic_t quit_signal = false;

bool get_abs_info(Config* config, int device_fd, int abs, struct input_absinfo* info) {
	if (ioctl(device_fd, EVIOCGABS(abs), info)) {
		logprintf(config->log, LOG_INFO, "Failed to find absolute axis %d\n", abs);
		return false;
	}
	return true;
}

bool send_abs_info(int sock_fd, int device_fd, Config* config) {

	int keys[] = {
		ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y
	};

	int i;
	ABSInfoMessage msg = {
		.msg_type = MESSAGE_ABSINFO
	};
	for (i = 0; i < sizeof(keys) / sizeof(int); i++) {
		msg.axis = keys[i];
		memset(&msg.info, 0, sizeof(msg.info));
		if (!get_abs_info(config, device_fd, keys[i], &msg.info)) {
			continue;
		}

		if (msg.info.minimum != msg.info.maximum) {
			if (!send_message(config->log, sock_fd, &msg, sizeof(msg))) {
				return false;
			}
		}
	}

	logprintf(config->log, LOG_DEBUG, "Absolute axes synchronized\n");
	return true;
}

bool setup_device(int sock_fd, int device_fd, Config* config) {
	ssize_t msglen = sizeof(DeviceMessage) + UINPUT_MAX_NAME_SIZE;
	DeviceMessage* msg = malloc(msglen);
	memset(msg, 0, msglen);

	msg->msg_type = MESSAGE_DEVICE;
	msg->length = UINPUT_MAX_NAME_SIZE;
	msg->type = htobe64(config->type);

	struct input_id ids;

	if (ioctl(device_fd, EVIOCGID, &ids) < 0) {
		logprintf(config->log, LOG_ERROR, "Failed to query device ID: %s\n", strerror(errno));
		free(msg);
		return false;
	}

	msg->id_bustype = htobe16(ids.bustype);
	msg->id_vendor = htobe16(ids.vendor);
	msg->id_product = htobe16(ids.product);
	msg->id_version = htobe16(ids.version);

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

	logprintf(config->log, LOG_DEBUG, "Initiating SETUP\n");
	if (!send_message(config->log, sock_fd, msg, msglen)) {
		free(msg);
		return false;
	}

	free(msg);

	if (!send_abs_info(sock_fd, device_fd, config)) {
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
		// msg_type byte + length byte + pwlen
		PasswordMessage* passwordMessage = malloc(2 + pwlen);

		passwordMessage->msg_type = MESSAGE_PASSWORD;
		passwordMessage->length = pwlen;
		strncpy(passwordMessage->password, config->password, pwlen);

		if (!send_message(config->log, sock_fd, passwordMessage, 2 + pwlen)) {
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

	logprintf(config->log, LOG_INFO, "Connected to slot: %d\n", buf[1]);
	config->slot = buf[1];

	return true;
}

void quit() {
	quit_signal = true;
}

int setType(int argc, char** argv, Config* config) {
	if (!strcmp(argv[1], "mouse")) {
		config->type |= DEV_TYPE_MOUSE;
	} else if (!strcmp(argv[1], "gamepad")) {
		config->type |= DEV_TYPE_GAMEPAD;
	} else if (!strcmp(argv[1], "keyboard")) {
		config->type |= DEV_TYPE_KEYBOARD;
	} else if (!strcmp(argv[1], "xbox")) {
		config->type |= DEV_TYPE_XBOX;
	} else {
		return -1;
	}

	printf("type: 0x%zx\n", config->type);

	return 1;
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
			"    -t, --type <type>       - Device type (required)\n"
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
	eargs_addArgument("-t", "--type", setType, 1);
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

int main(int argc, char** argv){

	Config config = {
		.log = {
			.stream = stderr,
			.verbosity = 0
		},
		.program_name = argv[0],
		.dev_name = NULL,
		.host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.type = 0,
		.slot = 0
	};

	int event_fd, sock_fd;
	ssize_t bytes;
	struct input_event event;
	DataMessage data = {0};
	data.msg_type = MESSAGE_DATA;

	add_arguments(&config);
	char* output[argc];
	int outputc = eargs_parse(argc, argv, output, &config);

	if(outputc < 1){
		logprintf(config.log, LOG_ERROR, "Missing arguments\n");
		return EXIT_FAILURE;
	}

	logprintf(config.log, LOG_INFO, "Reading input events from %s\n", output[0]);
	event_fd = open(output[0], O_RDONLY);
	if(event_fd < 0){
		logprintf(config.log, LOG_ERROR, "Failed to open device: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	struct sigaction act = {
		.sa_handler = &quit
	};

	if (sigaction(SIGINT, &act, NULL) < 0) {
		logprintf(config.log, LOG_ERROR, "Failed to set signal mask\n");
		return 10;
	}

	sock_fd = tcp_connect(config.host, config.port);
	if(sock_fd < 0) {
		logprintf(config.log, LOG_ERROR, "Failed to reach server at %s port %s\n", config.host, config.port);
		close(event_fd);
		return 2;
	}

	if (!init_connect(sock_fd, event_fd, &config)) {
		close(event_fd);
		close(sock_fd);
		return 3;
	}

	//get exclusive control
	int grab = 1;
 	if (ioctl(event_fd, EVIOCGRAB, &grab) < 0) {
		logprintf(config.log, LOG_WARNING, "Failed to request exclusive access to device: %s\n", strerror(errno));
		close(event_fd);
		close(sock_fd);
		return 4;
	}

	while(!quit_signal){
		//block on read
		bytes = read(event_fd, &event, sizeof(event));
		if(bytes < 0) {
			logprintf(config.log, LOG_ERROR, "read() failed: %s\nReconnecting...\n", strerror(errno));
			close(event_fd);
			event_fd = device_reopen(&config, output[0]);
			if (event_fd < 0) {
				break;
			} else {
				continue;
			}
		}
		if(bytes == sizeof(event)) {
			logprintf(config.log, LOG_DEBUG, "Event type:%d, code:%d, value:%d\n", event.type, event.code, event.value);

			data.type = htobe16(event.type);
			data.code = htobe16(event.code);
			data.value = htobe32(event.value);

			if(!send_message(config.log, sock_fd, &data, sizeof(data))) {
				//check if connection is closed
				if(errno == ECONNRESET || errno == EPIPE) {
					if (!init_connect(sock_fd, event_fd, &config)) {
						logprintf(config.log, LOG_ERROR, "Reconnection failed: %s\n", strerror(errno));
						break;
					}
				} else {
					logprintf(config.log, LOG_ERROR, "Failed to send: %s\n", strerror(errno));
					break;
				}
			}
		} else{
			logprintf(config.log, LOG_WARNING, "Short read from event descriptor (%zd bytes)\n", bytes);
		}
	}
	if (event_fd != -1) {
		close(event_fd);
	}
	if (sock_fd != -1) {
		uint8_t quit_msg = MESSAGE_QUIT;
		send_message(config.log, sock_fd, &quit_msg, sizeof(quit_msg));
		close(sock_fd);
	}

	return 0;
}

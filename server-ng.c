#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define SERVER_VERSION "GamepadServer 1.1"
#define MAX_CLIENTS 8
#define MAX_WAITING_CLIENTS 8

#include "libs/logger.h"
#include "libs/logger.c"
#include "libs/easy_args.h"
#include "libs/easy_args.c"
#include "network.h"
#include "protocol.h"
#include "structures.h"
#include "uinput.h"
#include "uinput.c"
#include "server-ng.h"

volatile sig_atomic_t shutdown_server = 0;
gamepad_client clients[MAX_CLIENTS] = {};

void signal_handler(int param) {
	shutdown_server = 1;
}

int client_close(LOGGER log, gamepad_client* client, bool cleanup){
//	if(cleanup){
		cleanup_device(log, client);
//	}
//	else{
		logprintf(log, LOG_INFO, "Closing client connection\n");
//	}

	if(client->fd >= 0){
		close(client->fd);
		client->fd = -1;
	}
	client->last_ret = 0;
	client->scan_offset = 0;
	return 0;
}

bool client_connection(Config* config, int listener, gamepad_client waiting_queue[MAX_WAITING_CLIENTS]){
	size_t client_ident;
	for(client_ident = 0;
			client_ident < MAX_WAITING_CLIENTS && waiting_queue[client_ident].fd >= 0;
			client_ident++) {}

	if(client_ident == MAX_WAITING_CLIENTS){
		logprintf(config->log, LOG_ERROR, "Client slots exhausted, turning connection away\n");
		int fd = accept(listener, NULL, NULL);
		uint8_t err = MESSAGE_CLIENT_SLOTS_EXHAUSTED;
		send_message(config->log, fd, &err, 1);
		close(fd);
		return true;
	}

	logprintf(config->log, LOG_INFO, "New client in slot %zu\n", client_ident);
	waiting_queue[client_ident].fd = accept(listener, NULL, NULL);

	return true;
}

bool create_node(LOGGER log, gamepad_client* client, struct device_meta* meta) {
	//generate libevdev device
	if(!create_device(log, client, meta)) {
		logprintf(log, LOG_ERROR, "Failed to create evdev node\n");
		//send_message(client->fd, "500 Cannot create evdev node\n");
		return false;
	}

	return true;
}

bool client_hello(Config* config, gamepad_client* client) {
	uint8_t ret = 0;

	if (client->bytes_available < sizeof(HelloMessage)) {
		logprintf(config->log, LOG_DEBUG, "not enough data");
		return true;
	}

	HelloMessage* msg = (HelloMessage*) client->input_buffer;

	if (msg->msg_type != MESSAGE_HELLO) {
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: Cannot handle MESSAGE_HANDLE here.\n");
		ret = MESSAGE_INVALID;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	if (msg->version != PROTOCOL_VERSION) {
		logprintf(config->log, LOG_DEBUG, "version mismatch: %.2x (client) != %.2x (server).\n", msg->version, PROTOCOL_VERSION);
		ret = MESSAGE_VERSION_MISMATCH;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	logprintf(config->log, LOG_DEBUG, "slot requested: %d\n", msg->slot);
	if (msg->slot > 0) {
		if (msg->slot - 1 > MAX_CLIENTS) {
			ret = MESSAGE_INVALID_CLIENT_SLOT;
			logprintf(config->log, LOG_WARNING, "invalid client slot: %d\n", msg->slot);
		} else if (clients[msg->slot - 1].fd > 0) {
			ret = MESSAGE_CLIENT_SLOT_IN_USE;
			logprintf(config->log, LOG_WARNING, "client slot in use: %d\n", msg->slot);
		}

		if (ret > 0) {
			send_message(config->log, client->fd, &ret, 1);
			close(client->fd);
			client->fd = -1;
			return false;
		}
	} else {
		int i;

		// check for free slot with no ev_fd
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i].fd < 0 && clients[i].ev_fd < 0) {
				msg->slot = i + 1;
				break;
			}
		}

		// check for free slot with ev_fd device and close the device
		if (msg->slot == 0) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (clients[i].fd < 0) {
					logprintf(config->log, LOG_INFO, "Free device\n");
					cleanup_device(config->log, clients + i);
					msg->slot = i + 1;
					break;
				}
			}
		}

		// client slots exhausted
		if (msg->slot == 0) {
			ret = MESSAGE_CLIENT_SLOTS_EXHAUSTED;
			send_message(config->log, client->fd, &ret, 1);
			close(client->fd);
			client->fd = -1;
			return false;
		}
	}
	if (strlen(config->password) > 0) {
		ret = MESSAGE_PASSWORD_REQUIRED;
	} else if (clients[msg->slot - 1].ev_fd == -1) {
		ret = MESSAGE_SETUP_REQUIRED;
	} else {
		ret = MESSAGE_SUCCESS;
	}

	if (!send_message(config->log, client->fd, &ret, 1)) {
		close(client->fd);
		client->fd = -1;
		return false;
	}

	logprintf(config->log, LOG_INFO, "hello complete\n");
	clients[msg->slot - 1].fd = client->fd;
	clients[msg->slot - 1].scan_offset = 0;
	clients[msg->slot - 1].bytes_available = 0;
	memset(clients[msg->slot - 1].input_buffer, 0, INPUT_BUFFER_SIZE);
	client->bytes_available = 0;
	client->scan_offset = 0;
	client->fd = -1;
	clients[msg->slot - 1].last_ret = ret;

	return true;
}

int usage(int argc, char** argv, Config* config) {
	printf("%s usage:\n"
			"%s [<options>]\n"
			"    -h,  --help                 - show this help\n"
			"    -p,  --port <port>          - sets the port\n"
			"    -b,  --bind <bind>          - sets the bind address\n"
			"    -pw, --password <password>  - sets the password\n",
			config->program_name, config->program_name);

	return -1;
}

bool add_arguments(Config* config) {
	eargs_addArgument("-h", "--help", usage, 0);
	eargs_addArgumentString("-p", "--port", &config->port);
	eargs_addArgumentString("-b", "--bind", &config->bindhost);
	eargs_addArgumentString("-pw", "--password", &config->password);
	eargs_addArgumentUInt("-v", "--verbosity", &config->log.verbosity);

	return true;
}

int handle_password(Config* config, gamepad_client* client, PasswordMessage* msg) {

	logprintf(config->log, LOG_DEBUG, "handle password\n");

	if (client->last_ret != MESSAGE_PASSWORD_REQUIRED) {
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: PasswordMessage must be send in PASSWORD_REQUIRED state.\n");
		return sizeof(PasswordMessage) + msg->length;
	}

	uint8_t message;

	if (strncmp(config->password, msg->password, msg->length)) {
		logprintf(config->log, LOG_WARNING, "INVALID_PASSWORD\n");
		message = MESSAGE_INVALID_PASSWORD;
	} else {
		if (client->ev_fd < 0) {
			message = MESSAGE_SETUP_REQUIRED;
		} else {
			message = MESSAGE_SUCCESS;
		}
	}

	client->last_ret = message;
	if (!send_message(config->log, client->fd, &message, sizeof(message))) {
		return -1;
	}

	return sizeof(PasswordMessage) + msg->length;
}

int handle_absinfo(Config* config, gamepad_client* client, ABSInfoMessage* msg) {
	logprintf(config->log, LOG_INFO, "Handle ABSInfo message.\n");
	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: ABSInfoMessage must be send in SETUP state.\n");
		return -1;
	}

	client->meta.absmax[msg->axis] = msg->info.maximum;
	client->meta.absmin[msg->axis] = msg->info.minimum;
	client->meta.absfuzz[msg->axis] = msg->info.fuzz;
	client->meta.absflat[msg->axis] = msg->info.flat;

	return sizeof(ABSInfoMessage);
}

int handle_device(Config* config, gamepad_client* client, DeviceMessage* msg) {
	logprintf(config->log, LOG_INFO, "handle device message.\n");

	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: DeviceMessage must be send in SETUP state.\n");
		return -1;
	}

	client->meta.devtype = msg->type;
	memcpy(&client->meta.id, &msg->ids, sizeof(struct input_id));
	client->meta.name = malloc(msg->length);
	memcpy(client->meta.name, msg->name, msg->length);

	return sizeof(DeviceMessage) + msg->length;
}

int handle_setup_required(Config* config, gamepad_client* client, uint8_t* message) {
	uint8_t msg;
	if (client->last_ret != MESSAGE_SUCCESS) {
		msg = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: SetupRequiredMessage must be send in SUCCESS state.\n");
		return -1;
	} else {
		msg = MESSAGE_SETUP_REQUIRED;
		client->last_ret = msg;
	}
	if (!send_message(config->log, client->fd, &msg, 1)) {
		return -1;
	}
	return 1;
}

int handle_quit(Config* config, gamepad_client* client, uint8_t* msg) {

	return 1;
}

int handle_setup_end(Config* config, gamepad_client* client, uint8_t* msg, uint8_t slot) {
	logprintf(config->log, LOG_DEBUG, "handle setup end\n");
	uint8_t message;
	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		message = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: SetupEndMessage must be send in SETUP state.\n");
		send_message(config->log, client->fd, &message, sizeof(message));
		return -1;
	} else {
		if (!create_device(config->log, client, &client->meta)) {
			return -1;
		}
		SuccessMessage message = {
			.msg_type = MESSAGE_SUCCESS,
			.slot = slot + 1
		};
		client->last_ret = MESSAGE_SUCCESS;

		if (!send_message(config->log, client->fd, &message, sizeof(message))) {
			return -1;
		}
	}

	return 1;
}


int handle_data(Config* config, gamepad_client* client, DataMessage* msg) {

	if (client->last_ret != MESSAGE_SUCCESS) {
		logprintf(config->log, LOG_WARNING, "MESSAGE_INVALID: DataMessage must be send in SUCCESS state.\n");
		return sizeof(DataMessage);
	}

	logprintf(config->log, LOG_DEBUG, "Type: 0x%.2x, code: 0x%.2x, value: 0x%.2x\n", msg->event.type, msg->event.code, msg->event.value);

	ssize_t bytes = write(client->ev_fd, &msg->event, sizeof(struct input_event));
	if (bytes < 0) {
		logprintf(config->log, LOG_ERROR, "Cannot write to device: %s\n", strerror(errno));
		return -1;
	}

	return sizeof(DataMessage);
}

bool client_data(Config* config, gamepad_client* client, uint8_t slot) {

	ssize_t bytes;
	uint8_t* msg;
	int ret;
	while (client->bytes_available > 0) {
		msg = client->input_buffer + client->scan_offset;

		bytes = get_size_from_command(msg, client->bytes_available);

		if (bytes < 0){
			logprintf(config->log, LOG_WARNING, "unkown message: 0x%.2x.\n", msg[0]);
			return false;
		}

		// we need additional bytes
		if (client->bytes_available < bytes) {
			logprintf(config->log, LOG_DEBUG, "additional bytes...\n");
			return true;
		}

		switch (msg[0]) {
			case MESSAGE_PASSWORD:
				ret = handle_password(config, client, (PasswordMessage*) msg);
				break;
			case MESSAGE_ABSINFO:
				ret = handle_absinfo(config, client, (ABSInfoMessage*) msg);
				break;
			case MESSAGE_DEVICE:
				ret = handle_device(config, client, (DeviceMessage*) msg);
				break;
			case MESSAGE_SETUP_REQUIRED:
				ret = handle_setup_required(config, client, msg);
				break;
			case MESSAGE_QUIT:
				ret = handle_quit(config, client, msg);
				break;
			case MESSAGE_SETUP_END:
				ret = handle_setup_end(config, client, msg, slot);
				break;
			case MESSAGE_DATA:
				ret = handle_data(config, client, (DataMessage*) msg);
				break;
			default:
				logprintf(config->log, LOG_ERROR, "Unkown message: 0x%.2x\n", msg[0]);
				ret = -1;
				break;
		}

		// error in handling message
		if (ret < 0) {
			close(client->fd);
			client->fd = -1;
			return false;
		}

		// update
		if (ret > 0) {
			client->scan_offset += ret;
			client->bytes_available -= ret;
			logprintf(config->log, LOG_DEBUG, "Update offsets to (%d, %d).\n", client->scan_offset, client->bytes_available);
		}
	}
	return true;
}

bool recv_data(Config* config, gamepad_client* client) {
	logprintf(config->log, LOG_DEBUG, "move %d bytes.\n", client->bytes_available);
	memmove(client->input_buffer, client->input_buffer + client->scan_offset, client->bytes_available);

	ssize_t bytes;

	bytes = recv(client->fd, client->input_buffer + client->bytes_available, INPUT_BUFFER_SIZE - client->bytes_available, 0);

	// cannot receive data
	if (bytes < 0) {
		logprintf(config->log, LOG_ERROR, "Cannot recveice data from socket.\n");
		return false;
	}

	if (bytes == 0) {
		logprintf(config->log, LOG_ERROR, "Client closes connection.\n");
		return false;
	}
	logprintf(config->log, LOG_DEBUG, "%d bytes received\n", bytes);

	client->bytes_available += bytes;
	client->scan_offset = 0;

	return true;
}

void init_client(gamepad_client* client) {
	client->fd = -1;
	client->ev_fd = -1;

	client->scan_offset = 0;
	client->bytes_available = 0;
}

int main(int argc, char** argv) {
	size_t u;
	fd_set readfds;
	int maxfd;
	int status;
	Config config = {
		.program_name = argv[0],
		.log = {
			.stream = stderr,
			.verbosity = 5
		},
		.bindhost = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD
	};

	add_arguments(&config);
	status = eargs_parse(argc, argv, NULL, &config);
	if (status < 0) {
		return 1;
	} else if (status > 0) {
		logprintf(config.log, LOG_ERROR, "Unkown command line arguments.\n");
		return usage(argc, argv, &config);
	}

	logprintf(config.log, LOG_INFO, "%s starting\nProtocol Version: %.2x\n", SERVER_VERSION, PROTOCOL_VERSION);
	int listen_fd = tcp_listener(config.bindhost, config.port);
	if(listen_fd < 0){
		logprintf(config.log, LOG_ERROR, "Failed to open listener\n");
		return EXIT_FAILURE;
	}

	//set up signal handling
	signal(SIGINT, signal_handler);

	//initialize all clients to invalid sockets
	for(u = 0; u < MAX_CLIENTS; u++){
		init_client(clients + u);
	}

	gamepad_client waiting_clients[MAX_WAITING_CLIENTS];
	for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
		init_client(waiting_clients + u);
	}

	logprintf(config.log, LOG_INFO, "Now waiting for connections on %s:%s\n", config.bindhost, config.port);

	//core loop
	while (!shutdown_server) {
		FD_ZERO(&readfds);
		FD_SET(listen_fd, &readfds);
		maxfd = listen_fd;
		for(u = 0; u < MAX_CLIENTS; u++){
			if(clients[u].fd >= 0){
				FD_SET(clients[u].fd, &readfds);
				maxfd = (maxfd > clients[u].fd) ? maxfd:clients[u].fd;
			}
		}

		for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
			if (waiting_clients[u].fd >= 0) {
				FD_SET(waiting_clients[u].fd, &readfds);
				maxfd = (maxfd > waiting_clients[u].fd ? maxfd : waiting_clients[u].fd);
			}
		}

		//wait for events
		status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if(status < 0){
			logprintf(config.log, LOG_ERROR, "Error in select: %s\n", strerror(errno));;
			shutdown_server = 1;
		}
		else{
			logprintf(config.log, LOG_DEBUG, "DATA incoming\n");
			if(FD_ISSET(listen_fd, &readfds)){
				logprintf(config.log, LOG_INFO, "new connection\n");
				//handle client connection
				client_connection(&config, listen_fd, waiting_clients);
			}
			for(u = 0; u < MAX_CLIENTS; u++){
				if(FD_ISSET(clients[u].fd, &readfds)){
					//handle client data
					if (!recv_data(&config, clients + u)) {
						close(clients[u].fd);
						clients[u].fd = -1;
						continue;
					}
					client_data(&config, clients + u, u);
				}
			}

			for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
				if (FD_ISSET(waiting_clients[u].fd, &readfds)) {
					//handle waiting clients
					if (!recv_data(&config, waiting_clients + u)) {
						logprintf(config.log, LOG_ERROR, "Error in recv data.\n");
						close(waiting_clients[u].fd);
						waiting_clients[u].fd = -1;
						continue;
					}
					logprintf(config.log, LOG_DEBUG, "handle hello\n");
					client_hello(&config, waiting_clients + u);
				}
			}
		}
	}

	for(u = 0; u < MAX_CLIENTS; u++){
		client_close(config.log, clients + u, true);
	}
	close(listen_fd);
	return EXIT_SUCCESS;
}

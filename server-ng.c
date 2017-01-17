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
	client->passthru = false;
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

bool set_abs_value_for(char* token, int code, struct device_meta* meta) {
	int len = strlen(token);
	if (!strncmp(token, "MIN", 3) && len > 4) {
		meta->absmin[code] = strtoul(token + 4, NULL, 10);
	} else if (!strncmp(token, "MAX", 3) && len > 4) {
		meta->absmax[code] = strtoul(token + 4, NULL, 10);
	} else if (!strncmp(token, "FLAT", 4) && len > 5) {
		meta->absflat[code] = strtoul(token + 5, NULL, 10);
	} else if (!strncmp(token, "FUZZ", 4) && len > 5) {
		meta->absfuzz[code] = strtoul(token + 5, NULL, 10);
	} else {
		return false;
	}
	return true;
}

bool set_abs_value(char* token, struct device_meta* meta) {
	int len = strlen(token);
	int code = -1;
	if (!strncmp(token, "X_", 2) && len > 2) {
		code = ABS_X;
		token += 2;
	} else if (!strncmp(token, "Y_", 2) && len > 2) {
		code = ABS_Y;
		token += 2;
	} else if (!strncmp(token , "Z_", 2) && len > 2) {
		code = ABS_Z;
		token += 2;
	} else if (!strncmp(token , "RX_", 3) && len > 3) {
		code = ABS_RX;
		token += 3;
	} else if (!strncmp(token , "RY_", 3) && len > 3) {
		code = ABS_RY;
		token += 3;
	} else if (!strncmp(token , "RZ_", 3) && len > 3) {
		code = ABS_RZ;
		token += 3;
	} else if (!strncmp(token , "HAT0X_", 6) && len > 6) {
		code = ABS_HAT0X;
		token += 6;
	} else if (!strncmp(token , "HAT0Y_", 6) && len > 6) {
		code = ABS_HAT0Y;
		token += 6;
	} else {
		return false;
	}

	return set_abs_value_for(token, code, meta);
}
/*
bool handle_hello(Config* config, gamepad_client* client) {
	char* token = strtok((char*) client->input_buffer, "\n");
	char* endptr;
	// help for device creation
	struct input_id id = {
		.vendor = 0x0000,
		.product = 0x0000,
		.version = 0x0001,
		.bustype = 0x0011
	};
	struct device_meta meta = {
		.id = id,
		.devtype = DEV_TYPE_UNKOWN,
		.name = "",
		.absmax = {0},
		.absmin = {0},
		.absflat = {0},
		.absfuzz = {0}
	};

	init_abs_info(&meta);

	while(token != NULL && strlen(token) > 0) {

		// hello followed by the protocol version
		// HELLO <version>
		if (!strncmp(token, "HELLO ", 6)) {
			if (strcmp(token + 6, PROTOCOL_VERSION)) {
				logprintf(config->log, LOG_INFO, "Disconnecting client with invalid protocol version %s\n", token);
				send_message(client->fd, "400 Protocol version mismatch\n");
				return false;
			}
		} else if (!strncmp(token, "ABS_", 4) && strlen(token) > 4) {
			if (!set_abs_value(token + 4, &meta)) {
				logprintf(config->log, LOG_INFO, "Cannot parse ABS_: %s\n", token);
				send_message(client->fd, "400 Cannot parse ABS value\n");
				return false;
			}
		// vendor id of the device
		// VENDOR 0xXXXX
		} else if (!strncmp(token, "VENDOR ", 7)) {
			meta.id.vendor = strtol(token + 7, &endptr, 16);
			if (token + 7 == endptr) {
				logprintf(config->log, LOG_INFO, "vendor_id was not a valid number (%s).\n", token + 7);
				send_message(client->fd, "400 Cannot parse vendor id\n");
				return false;
			}
		// product id of the device
		// PRODUCT 0xXXXX
		} else if (!strncmp(token, "PRODUCT ", 8)) {
			meta.id.product = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				logprintf(config->log, LOG_INFO, "product_id was not a valid number (%s).\n", token + 8);
				send_message(client->fd, "400 Cannot parse product id\n");
				return false;
			}
		// bus type of the device
		// BUSTYPE 0xXXXX
		} else if (!strncmp(token, "BUSTYPE ", 8)) {
			meta.id.bustype = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				logprintf(config->log, LOG_INFO, "bustype was not a valid number (%s).\n", token + 8);
				send_message(client->fd, "400 Cannot parse bustype\n");
				return false;
			}
		// version of the device
		// VERSION 0xXXXX
		} else if (!strncmp(token, "VERSION ", 8)) {
			meta.id.version = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				logprintf(config->log, LOG_INFO, "version was not a valid number (%s).\n", token + 8);
				send_message(client->fd, "400 Cannot parse device version\n");
				return false;
			}

		// devtype of the device
		// for mapping see DEV_TYPE
		// DEVTYPE <number>
		} else if (!strncmp(token, "DEVTYPE ", 8)) {
			meta.devtype = strtol(token + 8, &endptr, 10);
			if (token + 7 == endptr) {
				logprintf(config->log, LOG_INFO, "devtype was not a valid number (%s).\n", token + 8);
				send_message(client->fd, "400 Cannot parse device type\n");
				return false;
			}
		// name of the device
		// NAME <name>
		} else if (!strncmp(token, "NAME ", 5)) {
			meta.name = token + 5;
		// password for this server
		// PASSWORD <password>
		} else if (!strncmp(token, "PASSWORD ", 9)) {
			if (strcmp(token + 9, config->password)) {
				logprintf(config->log, LOG_INFO, "Disconnecting client with invalid access token\n");
				send_message(client->fd, "401 Incorrect password or token\n");
				return false;
			}
		} else {
			logprintf(config->log, LOG_INFO, "Unkown command: %s\n", token);
			send_message(client->fd, "400 Unkown command\n");
			return false;
		}

		token = strtok(NULL, "\n");
	}

	return create_node(config->log, client, &meta);
}
*/
bool client_hello(Config* config, gamepad_client* client) {
	uint8_t ret;

	HelloMessage msg = {0};
	if (client->bytes_available < sizeof(msg)) {
		return true;
	}

	memcpy(&msg, client->input_buffer, sizeof(msg));

	if (msg.msg_type != MESSAGE_HELLO) {
		ret = MESSAGE_INVALID_COMMAND;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	if (msg.version != BINARY_PROTOCOL_VERSION) {
		ret = MESSAGE_VERSION_MISMATCH;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
	}

	if (msg.slot > 0) {
		if (msg.slot > MAX_CLIENTS) {
			ret = MESSAGE_CLIENT_SLOT_TOO_HIGH;
		} else if (clients[msg.slot].fd != -1) {
			ret = MESSAGE_CLIENT_SLOT_IN_USE;
		}
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
	} else {
		int i;

		// check for free slot with no ev_fd
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i].fd == -1 && clients[i].ev_fd == -1) {
				msg.slot = i;
				break;
			}
		}

		// check for free slot with ev_fd device and close the device
		if (msg.slot == 0) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (clients[i].fd == -1) {
					cleanup_device(config->log, clients + i);
					msg.slot = i;
					break;
				}
			}
		}
	}
	if (strlen(config->password) > 0) {
		ret = MESSAGE_PASSWORD_REQUIRED;
	} else if (clients[msg.slot].ev_fd == -1) {
		ret = MESSAGE_SETUP_REQUIRED;
	} else {
		clients[msg.slot].passthru = true;
		ret = MESSAGE_SUCCESS;
	}

	if (!send_message(config->log, client->fd, &ret, 1)) {
		close(client->fd);
		client->fd = -1;
	}
	clients[msg.slot].fd = client->fd;
	clients[msg.slot].scan_offset = 0;
	clients[msg.slot].bytes_available = 0;
	client->bytes_available = 0;
	client->scan_offset = 0;
	client->fd = -1;
	clients[msg.slot].last_ret = ret;

	return true;
}
/*
int client_data(Config* config, gamepad_client* client){
	ssize_t bytes;
	size_t u;
	struct input_event* event = (struct input_event*) client->input_buffer;

	bytes = recv(client->fd, client->input_buffer + client->scan_offset, sizeof(client->input_buffer) - client->scan_offset, 0);

	//check if closed
	if(bytes < 0){
		logprintf(config->log, LOG_ERROR, "Error on recviece: %s\n", strerror(errno));
		return client_close(config->log, client, false);
	}
	else if(bytes == 0){
		return client_close(config->log, client, false);
	}

	client->scan_offset += bytes;

	//check for overfull buffer
	if(sizeof(client->input_buffer) - client->scan_offset < 10){
		logprintf(config->log, LOG_WARNING, "Disconnecting spammy client\n");
		return client_close(config->log, client, false);
	}

	if(!client->passthru){
		//protocol negotiation
		if(client->scan_offset >= strlen("HELLO ")){
			//check for message end
			for(u = 0; u < client->scan_offset && client->input_buffer[u]; u++){
			}
			if(u < client->scan_offset){
				if(!strncmp((char*) client->input_buffer, "HELLO ", 6)) {
					if (!handle_hello(config, client)) {
						return client_close(config->log, client, true);
					}
				} else if (!strncmp((char*) client->input_buffer, "CONTINUE ", 9)) {
					if(strcmp((char*) client->input_buffer + 9, client->token)){
						logprintf(config->log, LOG_INFO, "Disconnecting client with invalid access token\n");
						send(client->fd, "401 Incorrect password or token\0", 32, 0);
						return client_close(config->log, NULL, true);
					}
				} else {
						logprintf(config->log, LOG_INFO, "Disconnecting client with invalid access token\n");
						send(client->fd, "401 Incorrect password or token\0", 32, 0);
						return client_close(config->log, client, true);
				}
				//update offset
				client->scan_offset -= (u + 1);
				//copy back
				memmove(client->input_buffer, client->input_buffer + u + 1, client->scan_offset);
				//enable passthru
				client->passthru = true;
				//notify client
				send(client->fd, "200 ", 4, 0);
				send(client->fd, client->token, strlen(client->token), 0);
				send(client->fd, "\n", 1, 0);
				logprintf(config->log, LOG_INFO, "Client passthrough enabled with %zu bytes of data left\n", client->scan_offset);
				return true;

			} else {
				logprintf(config->log, LOG_ERROR, "Disconnecting non-conforming client\n");
				send(client->fd, "500 Unknown greeting\0", 21, 0);
				return client_close(config->log, client, true);
			}
		}
	}
	//handle message
	else{
		//if complete message, push to node
		while(client->scan_offset >= sizeof(struct input_event)){
			//send message
			write(client->ev_fd, event, sizeof(struct input_event));
			logprintf(config->log, LOG_DEBUG, "Writing event: client:%zu, type:%d, code:%d, value:%d\n", client - clients, event->type, event->code, event->value);
			//update offset
			client->scan_offset -= sizeof(struct input_event);
			//copy back
			memmove(client->input_buffer, client->input_buffer + sizeof(struct input_event), client->scan_offset);
		}
	}

	return 0;
}
*/
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

int client_data(Config* config, gamepad_client* client) {

	return 0;
}

bool recv_data(Config* config, gamepad_client* client) {
	memmove(client->input_buffer, client->input_buffer + client->scan_offset, client->bytes_available);

	ssize_t bytes;

	bytes = recv(client->fd, client->input_buffer + client->bytes_available, sizeof(client->input_buffer) - client->bytes_available, 0);

	if (bytes < 0) {
		return false;
	}

	client->bytes_available += bytes;
	client->scan_offset = 0;

	return true;
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

	logprintf(config.log, LOG_INFO, "%s starting\nProtocol Version: %s\n", SERVER_VERSION, PROTOCOL_VERSION);
	int listen_fd = tcp_listener(config.bindhost, config.port);
	if(listen_fd < 0){
		fprintf(stderr, "Failed to open listener\n");
		return EXIT_FAILURE;
	}

	//set up signal handling
	signal(SIGINT, signal_handler);

	//initialize all clients to invalid sockets
	for(u = 0; u < MAX_CLIENTS; u++){
		clients[u].fd = -1;
		clients[u].ev_fd = -1;
	}

	gamepad_client waiting_clients[MAX_WAITING_CLIENTS];
	for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
		waiting_clients[u].fd = -1;
		waiting_clients[u].ev_fd = -1;
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
			if(FD_ISSET(listen_fd, &readfds)){
				logprintf(config.log, LOG_INFO, "new connection\n");
				//handle client connection
				client_connection(&config, listen_fd, waiting_clients);
			}
			for(u = 0; u < MAX_CLIENTS; u++){
				if(FD_ISSET(clients[u].fd, &readfds)){
					//handle client data
					if (!recv_data(&config, waiting_clients + u)) {
						close(waiting_clients[u].fd);
						waiting_clients[u].fd = -1;
						continue;
					}
					client_data(&config, clients + u);
				}
			}

			for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
				if (FD_ISSET(waiting_clients[u].fd, &readfds)) {
					//handle waiting clients
					if (recv_data(&config, waiting_clients + u)) {
						close(waiting_clients[u].fd);
						waiting_clients[u].fd = -1;
						continue;
					}
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

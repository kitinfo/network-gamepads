#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define SERVER_VERSION "GamepadServer 1.1"
#define MAX_CLIENTS 8
#define MAX_WAITING_CLIENTS 8

#include "../libs/easy_args.h"
#include "../libs/logger.h"

#include "../common/strdup.h"
#include "../common/network.h"
#include "../common/protocol.h"
#include "../common/structures.h"
#include "../common/uinput.h"


#include "input-server.h"

volatile sig_atomic_t shutdown_server = 0;
gamepad_client clients[MAX_CLIENTS] = {};

void signal_handler(int param) {
	shutdown_server = 1;
}

int client_close(LOGGER log, gamepad_client* client, uint8_t slot, bool cleanup){
	if(cleanup){
		cleanup_device(log, client);
	}

	logprintf(log, LOG_INFO, "client %d: Closing client connection\n", slot);

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
		if (fd < 0) {
			return false;
		}
		uint8_t err = MESSAGE_CLIENT_SLOTS_EXHAUSTED;
		send_message(config->log, fd, &err, 1);
		close(fd);
		return true;
	}

	logprintf(config->log, LOG_INFO, "New client in waiting slot %zu\n", client_ident);
	waiting_queue[client_ident].fd = accept(listener, NULL, NULL);

	return true;
}

bool client_hello(Config* config, gamepad_client* client, uint8_t slot) {
	uint8_t ret = 0;

	if (client->bytes_available < sizeof(HelloMessage)) {
		logprintf(config->log, LOG_DEBUG, "waiting slot %d: not enough data\n", slot);
		return true;
	}

	HelloMessage* msg = (HelloMessage*) client->input_buffer;

	if (msg->msg_type != MESSAGE_HELLO) {
		logprintf(config->log, LOG_WARNING, "waiting slot %d: MESSAGE_INVALID: Cannot handle MESSAGE_HANDLE here.\n", slot);
		ret = MESSAGE_INVALID;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	// protocol version must be the same
	if (msg->version != PROTOCOL_VERSION) {
		logprintf(config->log, LOG_DEBUG,
				"waiting slot %d: version mismatch: %.2x (client) != %.2x (server).\n",
				slot, msg->version, PROTOCOL_VERSION);
		ret = MESSAGE_VERSION_MISMATCH;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	logprintf(config->log, LOG_DEBUG, "waiting slot %d: slot requested: %d\n", slot, msg->slot);
	if (msg->slot > 0) {
		if (msg->slot > MAX_CLIENTS) {
			ret = MESSAGE_INVALID_CLIENT_SLOT;
			logprintf(config->log, LOG_WARNING, "waiting slot %d: invalid client slot: %d\n", slot, msg->slot);
		} else if (clients[msg->slot - 1].fd > 0) {
			ret = MESSAGE_CLIENT_SLOT_IN_USE;
			logprintf(config->log, LOG_WARNING, "waiting slot %d: client slot in use: %d\n", slot, msg->slot);
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
					logprintf(config->log, LOG_INFO, "waiting slot %d: Free old device slot %d\n", slot, i);
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

	// check if the server has set a password
	if (strlen(config->password) > 0) {
		ret = MESSAGE_PASSWORD_REQUIRED;
	// check if the device is already set up
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

	// move the client data to the right slot
	logprintf(config->log, LOG_INFO, "waiting slot: %d: hello complete\n", slot);
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
			"    -b,  --bind <bind>          - sets the bind address\n"
			"    -l,  --limit <type>         - limit device types to the given one\n"
			"                                  type is mouse, keyboard or gamepad\n"
			"    -h,  --help                 - show this help\n"
			"    -p,  --port <port>          - sets the port\n"
			"    -pw, --password <password>  - sets the password\n"
			"    -v,  --verbosity <level>    - sets the level of verbosity (0: ERROR - 4: ALL_IO)\n"
			, config->program_name, config->program_name);

	return -1;
}

int set_limit(int argc, char** argv, Config* config) {
	if (!strcmp(argv[1], "mouse")) {
		config->limit |= DEV_TYPE_MOUSE;
	} else if (!strcmp(argv[1], "keyboard")) {
		config->limit |= DEV_TYPE_KEYBOARD;
	} else if (!strcmp(argv[1], "gamepad")) {
		config->limit |= DEV_TYPE_GAMEPAD;
	} else if (!strcmp(argv[1], "xbox")) {
		config->limit |= DEV_TYPE_XBOX;
	} else {
		logprintf(config->log, LOG_ERROR,
				"unkown device type %s. Valid ones are: gamepad, keyboard, mouse\n", argv[1]);
		return -1;
	}

	return 1;
}

bool add_arguments(Config* config) {
	eargs_addArgument("-h", "--help", usage, 0);
	eargs_addArgument("-l", "--limit", set_limit, 1);
	eargs_addArgumentString("-p", "--port", &config->port);
	eargs_addArgumentString("-b", "--bind", &config->bindhost);
	eargs_addArgumentString("-pw", "--password", &config->password);
	eargs_addArgumentUInt("-v", "--verbosity", &config->log.verbosity);

	return true;
}

// handles a password message. Returns the bytes used or -1 on failure.
int handle_password(Config* config, gamepad_client* client, PasswordMessage* msg, uint8_t slot) {

	logprintf(config->log, LOG_DEBUG, "client %d: handle password\n", slot);

	if (client->last_ret != MESSAGE_PASSWORD_REQUIRED) {
		logprintf(config->log, LOG_WARNING,
				"client %d: MESSAGE_INVALID: PasswordMessage must be send in PASSWORD_REQUIRED state.\n", slot);
		return sizeof(PasswordMessage) + msg->length;
	}

	uint8_t message;

	if (strncmp(config->password, msg->password, msg->length)) {
		logprintf(config->log, LOG_WARNING, "client %d: INVALID_PASSWORD\n", slot);
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

// Handles a absinfo message. Returns the bytes used or -1 on failure.
int handle_absinfo(Config* config, gamepad_client* client, ABSInfoMessage* msg, uint8_t slot) {
	logprintf(config->log, LOG_INFO, "[%d] Absolute axis setup\n", slot);
	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		return -1;
	}

	if(msg->axis >= ABS_CNT){
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol data out of bounds\n", slot);
		return -1;
	}
	client->meta.absinfo[msg->axis] = msg->info;
	return sizeof(ABSInfoMessage);
}

// handles the device message. Returns the bytes used or -1 on failure.
int handle_device(Config* config, gamepad_client* client, DeviceMessage* msg, uint8_t slot) {
	logprintf(config->log, LOG_INFO, "[%d] Device setup\n", slot);

	// the device message may only be send in setup required state.
	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		return -1;
	}

	client->meta.devtype = be64toh(msg->type);

	client->meta.name = malloc(msg->length);
	memcpy(client->meta.name, msg->name, msg->length);

	return sizeof(DeviceMessage) + msg->length;
}

// handles the setup required message. Returns the bytes used or -1 on failure.
int handle_setup_required(Config* config, gamepad_client* client, uint8_t* message, uint8_t slot) {
	uint8_t msg;

	// setup required may only be send in success state.
	if (client->last_ret != MESSAGE_SUCCESS) {
		msg = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
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

// handles the quit message. Returns the bytes used or -1 on failure.
int handle_quit(Config* config, gamepad_client* client, uint8_t* msg, uint8_t slot) {
	client_close(config->log, client, slot, true);
	return -1;
}

// handles the setup end message. Returns the bytes used or -1 on failure.
int handle_setup_end(Config* config, gamepad_client* client, uint8_t* msg, uint8_t slot) {
	logprintf(config->log, LOG_DEBUG, "[%d] Setup done\n", slot);
	uint8_t message;
	// setup end message may only be send in setup required state.
	if (client->last_ret != MESSAGE_SETUP_REQUIRED) {
		message = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		send_message(config->log, client->fd, &message, sizeof(message));
		return -1;
	}
	// check for device limitations
	if (!(client->meta.devtype & config->limit)) {
		logprintf(config->log, LOG_WARNING, "[%d] Device type 0x%zx not enabled\n", slot, client->meta.devtype);

		uint8_t msg_na = MESSAGE_DEVICE_NOT_ALLOWED;
		send_message(config->log, client->fd, &msg_na, sizeof(msg_na));
		return -1;
	}
	if (!create_device(config->log, client, &client->meta)) {
		return -1;
	}
	SuccessMessage msg_succ = {
		.msg_type = MESSAGE_SUCCESS,
		.slot = slot + 1
	};
	client->last_ret = MESSAGE_SUCCESS;
	if (!send_message(config->log, client->fd, &msg_succ, sizeof(msg_succ))) {
		return -1;
	}

	return 1;
}

// handles data messages. Returns the bytes used or -1 on failure.
int handle_data(Config* config, gamepad_client* client, DataMessage* msg, uint8_t slot) {
	if (client->last_ret != MESSAGE_SUCCESS) {
		logprintf(config->log, LOG_WARNING, "[%d] Protocol error\n", slot);
		return sizeof(DataMessage);
	}

	struct input_event event = {
		.time = {0},
		.type = be16toh(msg->type),
		.code = be16toh(msg->code),
		.value = be32toh(msg->value)
	};

	logprintf(config->log, LOG_DEBUG,
			"[%d] Type: 0x%.2x Code: 0x%.2x Value: 0x%.2x\n", slot, event.type, event.code, event.value);

	ssize_t bytes = write(client->ev_fd, &event, sizeof(struct input_event));
	if (bytes < 0) {
		logprintf(config->log, LOG_ERROR, "[%d:] Failed to write event: %s\n", slot, strerror(errno));
		return -1;
	}

	return sizeof(DataMessage);
}

/**
 * Handles data from socket except the hello message. Hello message is handled in the hello_data function.
 */
bool client_data(Config* config, gamepad_client* client, uint8_t slot) {

	ssize_t bytes;
	uint8_t* msg;
	int ret;
	while (client->bytes_available > 0) {
		msg = client->input_buffer + client->scan_offset;

		bytes = get_size_from_command(msg, client->bytes_available);

		if (bytes < 0){
			logprintf(config->log, LOG_WARNING, "[%d] Invalid message: 0x%.2x.\n", slot, msg[0]);
			return false;
		}

		// we need additional bytes
		if (client->bytes_available < bytes) {
			logprintf(config->log, LOG_DEBUG, "[%d] Short read\n", slot);
			return true;
		}

		// handle messages
		switch (msg[0]) {
			case MESSAGE_PASSWORD:
				ret = handle_password(config, client, (PasswordMessage*) msg, slot);
				break;
			case MESSAGE_ABSINFO:
				ret = handle_absinfo(config, client, (ABSInfoMessage*) msg, slot);
				break;
			case MESSAGE_DEVICE:
				ret = handle_device(config, client, (DeviceMessage*) msg, slot);
				break;
			case MESSAGE_SETUP_REQUIRED:
				ret = handle_setup_required(config, client, msg, slot);
				break;
			case MESSAGE_QUIT:
				handle_quit(config, client, msg, slot);
				return false;
			case MESSAGE_SETUP_END:
				ret = handle_setup_end(config, client, msg, slot);
				break;
			case MESSAGE_DATA:
				ret = handle_data(config, client, (DataMessage*) msg, slot);
				break;
			default:
				logprintf(config->log, LOG_ERROR, "[%d] Unkown message type 0x%.2x\n", slot, msg[0]);
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
			logprintf(config->log, LOG_DEBUG, "[%d] Buffer updated to scan_offset %d, %d bytes left\n", slot, client->scan_offset, client->bytes_available);
		}
	}
	return true;
}

/**
 * moves the buffer of a client to the beginning and receives new data from the socket.
 * Returns false when a error occurs on receiving data from socket.
 */
bool recv_data(Config* config, gamepad_client* client, uint8_t slot) {
	memmove(client->input_buffer, client->input_buffer + client->scan_offset, client->bytes_available);

	ssize_t bytes;

	bytes = recv(client->fd, client->input_buffer + client->bytes_available, INPUT_BUFFER_SIZE - client->bytes_available, 0);

	// cannot receive data
	if (bytes < 0) {
		logprintf(config->log, LOG_ERROR, "[%d] Failed to receive data\n", slot);
		return false;
	}

	if (bytes == 0) {
		logprintf(config->log, LOG_ERROR, "[%d] Connection closed by remote\n", slot);
		return false;
	}
	logprintf(config->log, LOG_DEBUG, "[%d] %zd bytes received\n", slot, bytes);

	client->bytes_available += bytes;
	client->scan_offset = 0;

	return true;
}

// initializes the gamepad_client struct
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

	// init config struct
	Config config = {
		.program_name = argv[0],
		.log = {
			.stream = stderr,
			.verbosity = 0
		},
		.limit = 0,
		.bindhost = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD
	};

	// argument parsing
	add_arguments(&config);
	status = eargs_parse(argc, argv, NULL, &config);

	// error in parsing arguments
	if (status < 0) {
		return 1;
	} else if (status > 0) {
		logprintf(config.log, LOG_ERROR, "Unknown command line arguments.\n");
		return usage(argc, argv, &config);
	}

	// enable all devices if no limit has been set
	if (config.limit == 0) {
		config.limit = UINT64_MAX;
		logprintf(config.log, LOG_INFO, "Enabling all devices (Mask 0x%zx)\n", config.limit);
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
	// initialize all waiting slots to invalid sockets
	for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
		init_client(waiting_clients + u);
	}

	logprintf(config.log, LOG_INFO, "Now waiting for connections on %s:%s\n", config.bindhost, config.port);

	//core loop
	while (!shutdown_server) {
		FD_ZERO(&readfds);
		FD_SET(listen_fd, &readfds);
		maxfd = listen_fd;

		// adding client slots
		for(u = 0; u < MAX_CLIENTS; u++){
			if(clients[u].fd >= 0){
				FD_SET(clients[u].fd, &readfds);
				maxfd = (maxfd > clients[u].fd) ? maxfd:clients[u].fd;
			}
		}

		// adding waiting slots
		for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
			if (waiting_clients[u].fd >= 0) {
				FD_SET(waiting_clients[u].fd, &readfds);
				maxfd = (maxfd > waiting_clients[u].fd ? maxfd : waiting_clients[u].fd);
			}
		}

		//wait for events
		status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if(status < 0){
			logprintf(config.log, LOG_ERROR, "Failed to select: %s\n", strerror(errno));;
			shutdown_server = 1;
		}
		else{
			if(FD_ISSET(listen_fd, &readfds)){
				//handle client connection
				client_connection(&config, listen_fd, waiting_clients);
			}
			for(u = 0; u < MAX_CLIENTS; u++){
				if(FD_ISSET(clients[u].fd, &readfds)){
					//handle client data
					if (!recv_data(&config, clients + u, u)) {
						client_close(config.log, clients + u, u, false);
						continue;
					}
					client_data(&config, clients + u, u);
				}
			}

			for (u = 0; u < MAX_WAITING_CLIENTS; u++) {
				if (FD_ISSET(waiting_clients[u].fd, &readfds)) {
					//handle waiting clients
					if (!recv_data(&config, waiting_clients + u, -u)) {
						close(waiting_clients[u].fd);
						waiting_clients[u].fd = -1;
						continue;
					}
					if (!client_hello(&config, waiting_clients + u, u)) {
						waiting_clients[u].bytes_available = 0;
						waiting_clients[u].scan_offset = 0;
					}
				}
			}
		}
	}

	for(u = 0; u < MAX_CLIENTS; u++){
		client_close(config.log, clients + u, u, true);
	}
	close(listen_fd);
	return EXIT_SUCCESS;
}

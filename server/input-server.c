#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <libevdev/libevdev.h>

#define SERVER_VERSION "InputServer 1.3"
#define MAX_CLIENTS 8
#define MAX_WAITING_CLIENTS 8

#include "../libs/easy_args.h"
#include "../libs/logger.h"

#include "../common/strdup.h"
#include "../common/network.h"
#include "../common/protocol.h"

#include "uinput.h"
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

	logprintf(log, LOG_INFO, "[%d] Closing client connection\n", slot);

	if(client->fd >= 0){
		close(client->fd);
		client->fd = -1;
	}
	client->status = MESSAGE_RESERVED_UNCONN;
	client->scan_offset = 0;

	//remove this, as when reconnecting we either reuse the old device or re-setup a new one
	//in the first case, we dont need to set the bits again
	//in the second case, we want a clean slate
	free(client->meta.enabled_events);
	client->meta.enabled_events = NULL;
	client->meta.enabled_events_length = 0;
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
	size_t u;

	if (client->bytes_available < sizeof(HelloMessage)) {
		logprintf(config->log, LOG_DEBUG, "[Wait%d] Short read\n", slot);
		return true;
	}

	HelloMessage* msg = (HelloMessage*) client->input_buffer;

	if (msg->msg_type != MESSAGE_HELLO) {
		logprintf(config->log, LOG_WARNING, "[Wait%d] Protocol error\n", slot);
		ret = MESSAGE_INVALID;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	// protocol version must be the same
	if (msg->version != PROTOCOL_VERSION) {
		logprintf(config->log, LOG_DEBUG,
				"[Wait%d] Version mismatch: %.2x (client) vs %.2x (server)\n",
				slot, msg->version, PROTOCOL_VERSION);
		ret = MESSAGE_VERSION_MISMATCH;
		send_message(config->log, client->fd, &ret, 1);
		close(client->fd);
		client->fd = -1;
		return false;
	}

	logprintf(config->log, LOG_DEBUG, "[Wait%d] Slot requested: %d\n", slot, msg->slot);
	if (msg->slot > 0) {
		if (msg->slot > MAX_CLIENTS) {
			ret = MESSAGE_INVALID_CLIENT_SLOT;
			logprintf(config->log, LOG_WARNING, "[Wait%d] Invalid slot supplied\n", slot);
		} else if (clients[msg->slot - 1].fd > 0) {
			ret = MESSAGE_CLIENT_SLOT_IN_USE;
			logprintf(config->log, LOG_WARNING, "[Wait%d] Slot occupied\n", slot);
		}

		if (ret > 0) {
			send_message(config->log, client->fd, &ret, 1);
			close(client->fd);
			client->fd = -1;
			return false;
		}
	} else {

		// check for free slot with no ev_fd
		for (u = 0; u < MAX_CLIENTS; u++) {
			if (clients[u].fd < 0 && clients[u].ev_fd < 0) {
				msg->slot = u + 1;
				break;
			}
		}

		// check for free slot with ev_fd device and close the device
		if (msg->slot == 0) {
			for (u = 0; u < MAX_CLIENTS; u++) {
				if (clients[u].fd < 0) {
					logprintf(config->log, LOG_INFO, "[Wait%d] Removing old device from allocated slot %d\n", slot, u);
					cleanup_device(config->log, clients + u);
					msg->slot = u + 1;
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
	logprintf(config->log, LOG_INFO, "[Wait%d] Connection negotiated\n", slot);
	clients[msg->slot - 1].fd = client->fd;
	clients[msg->slot - 1].scan_offset = 0;
	clients[msg->slot - 1].bytes_available = 0;
	memset(clients[msg->slot - 1].input_buffer, 0, INPUT_BUFFER_SIZE);
	client->bytes_available = 0;
	client->scan_offset = 0;
	client->fd = -1;
	clients[msg->slot - 1].status = ret;

	return true;
}

int usage(int argc, char** argv, Config* config) {
	printf("%s usage:\n"
			"%s [<options>]\n"
			"    -b,  --bind <bind>          - Address to bind to\n"
			"    -p,  --port <port>          - Port to use\n"
			"    -h,  --help                 - Print this help message\n"
			"    -pw, --password <password>  - Connection password\n"
			"    -v,  --verbosity <level>    - Verbosity level (0 (errors only) - 4 (all I/O))\n"
			, config->program_name, config->program_name);

	return -1;
}

int setCodeList(Config* config, char* file, int whitelist) {
	FILE* f = fopen(file, "r");
	char* line = NULL;
	char* type;
	char* code;
	size_t len = 0;
	if (f == NULL) {
		fprintf(stderr, "Cannot open file: %s\n", strerror(errno));
		return -1;
	}
	int status = 1;
	int itype;
	int icode;
	int line_num = 0;
	while (getline(&line, &len, f) != -1) {
		if (line[0] == '#') {
			continue;
		}
		line[strlen(line) -1] = 0;
		type = strtok(line, ".");
		code = strtok(NULL, ".");

		if (!type || !code) {
			logprintf(config->log, LOG_ERROR, "Line %d: Code not defined. Format is type.code or type.*\n", line_num);
			break;
		}
		itype = libevdev_event_type_from_name(type);

		if (itype < 0) {
			logprintf(config->log, LOG_ERROR, "Line %d: Type is not valid.\n", line_num);
			status = -1;
			break;
		}
		if (code[0] == '*') {
			logprintf(config->log, LOG_INFO, "Enable all events from type %s.\n", type);
			memset(config->whitelist[itype], 1, sizeof(int) * EV_KEY);
			continue;
		}

		icode = libevdev_event_code_from_name(itype, code);

		if (icode < 0) {
			logprintf(config->log, LOG_ERROR, "Line %d: Code is not valid.\n", line_num);
			status = -1;
			break;
		}

		logprintf(config->log, LOG_INFO, "Set %s.%s to %d\n", type, code, whitelist);

		config->whitelist[itype][icode] = whitelist;
		line_num++;
	}

	fclose(f);
	if (line) {
		free(line);
	}

	return status;
}

int setWhitelist(int argc, char** argv, Config* config) {
	return setCodeList(config, argv[1], 1);
}

int setBlacklist(int argc, char** argv, Config* config) {
	memset(config->whitelist, 1, sizeof(config->whitelist));
	return setCodeList(config, argv[1], 0);
}


bool add_arguments(Config* config) {
	eargs_addArgument("-h", "--help", usage, 0);
	eargs_addArgumentString("-p", "--port", &config->port);
	eargs_addArgumentString("-b", "--bind", &config->bindhost);
	eargs_addArgumentString("-pw", "--password", &config->password);
	eargs_addArgumentUInt("-v", "--verbosity", &config->log.verbosity);
	eargs_addArgument("-w", "--whitelist", setWhitelist, 1);
	eargs_addArgument("-B", "--blacklist", setBlacklist, 1);

	return true;
}

// handles a password message. Returns the bytes used or -1 on failure.
int handle_password(Config* config, gamepad_client* client, PasswordMessage* msg, uint8_t slot) {

	logprintf(config->log, LOG_DEBUG, "[%d] Validating authentication\n", slot);

	if (client->status != MESSAGE_PASSWORD_REQUIRED) {
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		return sizeof(PasswordMessage) + msg->length;
	}

	uint8_t message;

	if (strncmp(config->password, msg->password, msg->length)) {
		logprintf(config->log, LOG_WARNING, "[%d] Invalid password\n", slot);
		message = MESSAGE_INVALID_PASSWORD;
	} else {
		if (client->ev_fd < 0) {
			message = MESSAGE_SETUP_REQUIRED;
		} else {
			message = MESSAGE_SUCCESS;
		}
	}

	client->status = message;
	if (!send_message(config->log, client->fd, &message, sizeof(message))) {
		return -1;
	}

	return sizeof(PasswordMessage) + msg->length;
}

// Handles a absinfo message. Returns the bytes used or -1 on failure.
int handle_absinfo(Config* config, gamepad_client* client, ABSInfoMessage* msg, uint8_t slot) {
	logprintf(config->log, LOG_INFO, "[%d] Absolute axis %u setup\n", slot, msg->axis);
	if (client->status != MESSAGE_SETUP_REQUIRED) {
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
	if (client->status != MESSAGE_SETUP_REQUIRED) {
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		return -1;
	}

	client->meta.name = malloc(msg->length);
	memcpy(client->meta.name, msg->name, msg->length);

	return sizeof(DeviceMessage) + msg->length;
}

// handles the setup required message. Returns the bytes used or -1 on failure.
int handle_setup_required(Config* config, gamepad_client* client, uint8_t* message, uint8_t slot) {
	uint8_t msg;

	// setup required may only be send in success state.
	if (client->status != MESSAGE_SUCCESS) {
		msg = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		return -1;
	} else {
		msg = MESSAGE_SETUP_REQUIRED;
		client->status = msg;
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

int handle_request_event(Config* config, gamepad_client* client, RequestEventMessage* msg, uint8_t slot) {
	uint8_t message;

	if(client->status != MESSAGE_SETUP_REQUIRED){
		message = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING, "[%d] Protocol error\n", slot);
		send_message(config->log, client->fd, &message, sizeof(message));
		return -1;

	}
	if (msg->type >= EV_MAX) {
		logprintf(config->log, LOG_WARNING, "[%d] Event type is out of range.\n", slot);
		return -1;
	}

	if (msg->code >= KEY_MAX) {
		logprintf(config->log, LOG_WARNING, "[%d] Event code is out of range.\n", slot);
		return -1;
	}

	if (!config->whitelist[msg->type][msg->code]) {
		logprintf(config->log, LOG_WARNING, "[%d] Type %02X Code %X is forbidden.\n", slot, msg->type, msg->code);
		return sizeof(RequestEventMessage);
	}

	logprintf(config->log, LOG_DEBUG, "[%d] Enabling event type %02X code %X\n", slot, msg->type, msg->code);

	client->meta.enabled_events = realloc(client->meta.enabled_events, (client->meta.enabled_events_length + 1) * sizeof(struct enabled_event));
	if(!client->meta.enabled_events){
		fprintf(stderr, "Failed to allocate memory\n");
		client->meta.enabled_events_length = 0;
		return -1;
	}

	client->meta.enabled_events[client->meta.enabled_events_length].type = msg->type;
	client->meta.enabled_events[client->meta.enabled_events_length].code = msg->code;
	client->meta.enabled_events_length++;
	return sizeof(RequestEventMessage);
}

// handles the setup end message. Returns the bytes used or -1 on failure.
int handle_setup_end(Config* config, gamepad_client* client, uint8_t* msg, uint8_t slot) {
	logprintf(config->log, LOG_DEBUG, "[%d] Setup done\n", slot);
	uint8_t message;
	// setup end message may only be send in setup required state.
	if (client->status != MESSAGE_SETUP_REQUIRED) {
		message = MESSAGE_INVALID;
		logprintf(config->log, LOG_WARNING,
				"[%d] Protocol error\n", slot);
		send_message(config->log, client->fd, &message, sizeof(message));
		return -1;
	}
	if (!create_device(config->log, client, &client->meta)) {
		return -1;
	}
	SuccessMessage msg_succ = {
		.msg_type = MESSAGE_SUCCESS,
		.slot = slot + 1
	};
	client->status = MESSAGE_SUCCESS;
	if (!send_message(config->log, client->fd, &msg_succ, sizeof(msg_succ))) {
		return -1;
	}

	return 1;
}

// handles data messages. Returns the bytes used or -1 on failure.
int handle_data(Config* config, gamepad_client* client, DataMessage* msg, uint8_t slot) {
	if (client->status != MESSAGE_SUCCESS) {
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
			logprintf(config->log, LOG_DEBUG, "[%d] Short read, expected %zu\n", slot, bytes);
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
			case MESSAGE_REQUEST_EVENT:
				ret = handle_request_event(config, client, (RequestEventMessage*) msg, slot);
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
	gamepad_client empty = {
		.fd = -1,
		.ev_fd = -1
	};
	*client = empty;
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
		.bindhost = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST,
		.port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT,
		.password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD
	};

	memset(config.whitelist, 0, sizeof(config.whitelist));

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

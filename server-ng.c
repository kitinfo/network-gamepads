#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define SERVER_VERSION "GamepadServer 1.1"
#define MAX_CLIENTS 8

#include "network.h"
#include "protocol.h"
#include "evdev.h"
#include "structures.h"

volatile sig_atomic_t shutdown_server = 0;
char* global_password = NULL;
gamepad_client clients[MAX_CLIENTS] = {};

void signal_handler(int param) {
	shutdown_server = 1;
}

int client_close(gamepad_client* client, bool cleanup){
	if(cleanup){
		if(client->ev_input){
			libevdev_uinput_destroy(client->ev_input);
			client->ev_input = NULL;
		}

		if(client->ev_device){
			libevdev_free(client->ev_device);
			client->ev_device = NULL;
		}
		client->token[0] = 0;
		free(client->dev_name);
	}
	else{
		fprintf(stderr, "Closing client connection\n");
	}

	if(client->fd >= 0){
		close(client->fd);
		client->fd = -1;
	}
	client->passthru = false;
	client->scan_offset = 0;
	return 0;
}

int client_connection(int listener){
	size_t client_ident;
	unsigned u;
	for(client_ident = 0; client_ident < MAX_CLIENTS && clients[client_ident].fd >= 0; client_ident++){
	}

	if(client_ident == MAX_CLIENTS){
		//TODO no client slot left, turn away
		fprintf(stderr, "Client slots exhausted, turning connection away\n");
		return 0;
	}

	fprintf(stderr, "New client in slot %zu\n", client_ident);
	clients[client_ident].fd = accept(listener, NULL, NULL);

	//regenerate reconnection token
	if(!clients[client_ident].token[0]){
		for(u = 0; u < TOKEN_SIZE; u++){
			clients[client_ident].token[u] = rand() % 26 + 'a';
		}
	}
	return 0;
}

bool create_node(gamepad_client* client, struct device_meta* meta) {
	//generate libevdev device
	client->ev_device = evdev_node(meta);
	if(!client->ev_device){
		fprintf(stderr, "Failed to create evdev node\n");
		send(client->fd, "500 Cannot create evdev node\0", 29, 0);
		return false;
	}

	//generate libevdev input handle
	client->ev_input = evdev_input(client->ev_device);
	if(!client->ev_input){
		fprintf(stderr, "Failed to create input device\n");
		send(client->fd, "500 Cannot create input device\0", 31, 0);
		return false;
	}

	return true;
}
bool handle_hello(gamepad_client* client) {
	char* token = strtok((char*) client->input_buffer, "\n");
	char* endptr;
	// help for device creation
	struct device_meta meta = {
		.vendor_id = 0x0000,
		.product_id = 0x0000,
		.bustype = 0x0011,
		.devtype = DEV_TYPE_UNKOWN,
		.name = "",
		.version = 0x0001
	};

	while(token != NULL && strlen(token) > 0) {

		// hello followed by the protocol version
		// HELLO <version>
		if (!strncmp(token, "HELLO ", 6)) {
			if (strcmp(token + 6, PROTOCOL_VERSION)) {
				fprintf(stderr, "Disconnecting client with invalid protocol version %s\n", token);
				send(client->fd, "400 Protocol version mismatch\0", 30, 0);
				return false;
			}
		// vendor id of the device
		// VENDOR 0xXXXX
		} else if (!strncmp(token, "VENDOR ", 7)) {
			meta.vendor_id = strtol(token + 7, &endptr, 16);
			if (token + 7 == endptr) {
				fprintf(stderr, "vendor_id was not a valid number (%s).\n", token + 7);
				return false;
			}
		// product id of the device
		// PRODUCT 0xXXXX
		} else if (!strncmp(token, "PRODUCT ", 8)) {
			meta.product_id = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				fprintf(stderr, "product_id was not a valid number (%s).\n", token + 8);
				return false;
			}
		// bus type of the device
		// BUSTYPE 0xXXXX
		} else if (!strncmp(token, "BUSTYPE ", 8)) {
			meta.bustype = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				fprintf(stderr, "bustype was not a valid number (%s).\n", token + 8);
				return false;
			}
		// version of the device
		// VERSION 0xXXXX
		} else if (!strncmp(token, "VERSION ", 8)) {
			meta.version = strtol(token + 8, &endptr, 16);
			if (token + 7 == endptr) {
				fprintf(stderr, "version was not a valid number (%s).\n", token + 8);
				return false;
			}

		// devtype of the device
		// for mapping see DEV_TYPE
		// DEVTYPE <number>
		} else if (!strncmp(token, "DEVTYPE ", 8)) {
			meta.devtype = strtol(token + 8, &endptr, 10);
			if (token + 7 == endptr) {
				fprintf(stderr, "devtype was not a valid number (%s).\n", token + 8);
				return false;
			}
		// name of the device
		// NAME <name>
		} else if (!strncmp(token, "NAME ", 5)) {
			meta.name = token + 5;
		// password for this server
		// PASSWORD <password>
		} else if (!strncmp(token, "PASSWORD ", 9)) {
			if (strcmp(token + 9, global_password)) {
				fprintf(stderr, "Disconnecting client with invalid access token\n");
				send(client->fd, "401 Incorrect password or token\0", 32, 0);
				return false;
			}
		} else {
			fprintf(stderr, "Unkown command: %s\n", token);
			send(client->fd, "400 Unkown command\0", 19, 0);
			return false;
		}

		token = strtok(NULL, "\n");
	}

	return create_node(client, &meta);
}

int client_data(gamepad_client* client){
	ssize_t bytes;
	size_t u;
	struct input_event* event = (struct input_event*) client->input_buffer;

	bytes = recv(client->fd, client->input_buffer + client->scan_offset, sizeof(client->input_buffer) - client->scan_offset, 0);

	//check if closed
	if(bytes < 0){
		perror("recv");
		return client_close(client, false);
	}
	else if(bytes == 0){
		return client_close(client, false);
	}

	client->scan_offset += bytes;

	//check for overfull buffer
	if(sizeof(client->input_buffer) - client->scan_offset < 10){
		fprintf(stderr, "Disconnecting spammy client\n");
		return client_close(client, false);
	}

	if(!client->passthru){
		//protocol negotiation
		if(client->scan_offset >= strlen("HELLO ")){
			//check for message end
			for(u = 0; u < client->scan_offset && client->input_buffer[u]; u++){
			}
			if(u < client->scan_offset){
				if(!strncmp((char*) client->input_buffer, "HELLO ", 6)) {
					if (!handle_hello(client)) {
						return client_close(client, true);
					}
				} else if (!strncmp((char*) client->input_buffer, "CONTINUE ", 9)) {
					if(strcmp((char*) client->input_buffer + 9, client->token)){
						fprintf(stderr, "Disconnecting client with invalid access token\n");
						send(client->fd, "401 Incorrect password or token\0", 32, 0);
						return client_close(NULL, true);
					}
				} else {
						fprintf(stderr, "Disconnecting client with invalid access token\n");
						send(client->fd, "401 Incorrect password or token\0", 32, 0);
						return client_close(client, false);
				}
				//update offset
				client->scan_offset -= (u + 1);
				//copy back
				memmove(client->input_buffer, client->input_buffer + u + 1, client->scan_offset);
				//enable passthru
				client->passthru = true;
				//notify client
				send(client->fd, "200 ", 4, 0);
				send(client->fd, client->token, strlen(client->token) + 1, 0);
				fprintf(stderr, "Client passthrough enabled with %zu bytes of data left\n", client->scan_offset);
				return true;

			} else {
				fprintf(stderr, "Disconnecting non-conforming client\n");
				send(client->fd, "500 Unknown greeting\0", 21, 0);
				return client_close(client, true);
			}
		}
	}
	//handle message
	else{
		//if complete message, push to node
		while(client->scan_offset >= sizeof(struct input_event)){
			//send message
			libevdev_uinput_write_event(client->ev_input, event->type, event->code, event->value);
			fprintf(stderr, "Writing event: client:%zu, type:%d, code:%d, value:%d\n", client - clients, event->type, event->code, event->value);
			//update offset
			client->scan_offset -= sizeof(struct input_event);
			//copy back
			memmove(client->input_buffer, client->input_buffer + sizeof(struct input_event), client->scan_offset);
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	//FIXME at least the port should be done via argument
	size_t u;
	fd_set readfds;
	int maxfd;
	int status;
	char* bindhost = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST;
	char* port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT;
	global_password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD;
	fprintf(stderr, "%s starting\nProtocol Version: %s\n", SERVER_VERSION, PROTOCOL_VERSION);
	int listen_fd = tcp_listener(bindhost, port);
	if(listen_fd < 0){
		fprintf(stderr, "Failed to open listener\n");
		return EXIT_FAILURE;
	}

	//set up signal handling
	signal(SIGINT, signal_handler);

	//initialize all clients to invalid sockets
	for(u = 0; u < MAX_CLIENTS; u++){
		clients[u].fd = -1;
	}

	fprintf(stderr, "Now waiting for connections on %s:%s\n", bindhost, port);

	//core loop
	while(!shutdown_server){
		FD_ZERO(&readfds);
		FD_SET(listen_fd, &readfds);
		maxfd = listen_fd;
		for(u = 0; u < MAX_CLIENTS; u++){
			if(clients[u].fd >= 0){
				FD_SET(clients[u].fd, &readfds);
				maxfd = (maxfd > clients[u].fd) ? maxfd:clients[u].fd;
			}
		}

		//wait for events
		status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if(status < 0){
			perror("select");
			shutdown_server = 1;
		}
		else{
			if(FD_ISSET(listen_fd, &readfds)){
				//handle client connection
				client_connection(listen_fd);
			}
			for(u = 0; u < MAX_CLIENTS; u++){
				if(FD_ISSET(clients[u].fd, &readfds)){
					//handle client data
					client_data(clients + u);
				}
			}
		}
	}

	for(u = 0; u < MAX_CLIENTS; u++){
		client_close(clients + u, true);
	}
	close(listen_fd);
	return EXIT_SUCCESS;
}

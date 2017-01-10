#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include "network.h"
#include "protocol.h"

#define PROGRAM_NAME		"input-tools OSC translater 0.1"
#define DEFAULT_OSC_PORT	"8000"
#define DEFAULT_OSC_HOST	"::"

volatile sig_atomic_t shutdown_requested = 0;

typedef struct /*_2AXIS*/ {
	char* path;
	double min[2];
	double max[2];
} osc_2axis;

typedef struct /*_BTN*/ {
	char* path;
	double states[2];
} osc_button;

// THIS IS CURRENTLY PRETTY HACKY AND NOT USER FRIENDLY AT ALL

int input_negotiate(int fd, char* devname, char* password){
	char msg_buf[MSG_MAX * 4];
	ssize_t bytes, bytes_xfered;

	bytes = snprintf(msg_buf, sizeof(msg_buf), "HELLO %s\nDEVTYPE gamepad\nNAME %s\nPASSWORD %s\n\n", PROTOCOL_VERSION, devname, password);
	//FIXME unsafe send and undocumented 0 byte
	send(fd, msg_buf, bytes + 1, 0);

	bytes = 0;
	do{
		bytes_xfered = recv(fd, msg_buf + bytes, sizeof(msg_buf) - bytes, 0);
		if(bytes_xfered <= 0){
			perror("negotiate/recv");
			return -1;
		}
		bytes += bytes_xfered;
	}
	while(memchr(msg_buf, 0, bytes) == NULL);

	if(strtoul(msg_buf, NULL, 10) != 200){
		fprintf(stderr, "Server reported: %s\n", msg_buf);
		return -1;
	}

	fprintf(stderr, "Reconnection token (ignored): %s\n", msg_buf + 4);
	return 0;
}

int main(int argc, char** argv){
	//ext conf
	//name
	char* osc_port = getenv("OSC_PORT") ? getenv("OSC_PORT"):DEFAULT_OSC_PORT;
	char* osc_host = getenv("OSC_HOST") ? getenv("OSC_HOST"):DEFAULT_OSC_HOST;
	char* device_name = getenv("SERVER_NAME") ? getenv("SERVER_NAME"):"OSC Gamepad Bridge";
	char* input_host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST;
	char* input_port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT;
	char* password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD;

	int osc_fd, host_fd;

	fprintf(stderr, "%s\n", PROGRAM_NAME);

	osc_fd = udp_listener(osc_host, osc_port);
	if(osc_fd < 0){
		fprintf(stderr, "Failed to bind OSC port\n");
		return EXIT_FAILURE;
	}

	host_fd = tcp_connect(input_host, input_port);
	if(host_fd < 0){
		fprintf(stderr, "Failed to connect to input server\n");
		return EXIT_FAILURE;
	}

	//negotiate inputserver
	if(input_negotiate(host_fd, device_name, password) < 0){
		fprintf(stderr, "Failed to negotiate input-server protocol\n");
		return EXIT_FAILURE;
	}
	
	//configure osc input
	
	//xlate events

	
	fprintf(stderr, "Shutting down\n");
	close(osc_fd);
	close(host_fd);
	return EXIT_SUCCESS;
}

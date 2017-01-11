#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "network.h"
#include "protocol.h"

#define PROGRAM_NAME		"input-tools OSC translater 0.1"
#define DEFAULT_OSC_PORT	"8000"
#define DEFAULT_OSC_HOST	"::"

volatile sig_atomic_t shutdown_requested = 0;

typedef struct /*_OSC_CHANNEL*/{
	int code;
	double min;
	double max;
} osc_channel;

typedef struct /*_OSC_CONTROL*/{
	char* path;
	int type;
	unsigned num_channels;
	osc_channel channels[2];
} osc_control;

typedef struct /*_GP_CHANNEL*/{
	int code;
	double min;
	double max;
} gp_channel;

typedef struct /*_GP_CONTROL*/{
	char* name;
	int type;
	unsigned num_channels;
	gp_channel channels[2];
} gp_control;

gp_control gamepad_controls[]  = {
	{"Button A", EV_KEY, 1, {{BTN_A, 0, 1}}},
	{"Button B", EV_KEY, 1, {{BTN_B, 0, 1}}},
	{"Button X", EV_KEY, 1, {{BTN_X, 0, 1}}},
	{"Button Y", EV_KEY, 1, {{BTN_Y, 0, 1}}},
	{"Start Button", EV_KEY, 1, {{BTN_START, 0, 1}}},
	{"Select Button", EV_KEY, 1, {{BTN_SELECT, 0, 1}}},
	{"Left Trigger", EV_KEY, 1, {{BTN_TL, 0, 1}}},
	{"Right Trigger", EV_KEY, 1, {{BTN_TR, 0, 1}}},
	{"Left Thumb", EV_KEY, 1, {{BTN_THUMBL, 0, 1}}},
	{"Right Thumb", EV_KEY, 1, {{BTN_THUMBR, 0, 1}}},

	{"Left Stick", EV_ABS, 2, {{ABS_X, 0, 255}, {ABS_Y, 0, 255}}},
	{"Right Stick", EV_ABS, 2, {{ABS_RX, 0, 255}, {ABS_RY, 0, 255}}},
	{"Hat Stick", EV_ABS, 2, {{ABS_HAT0X, 0, 255}, {ABS_HAT0Y, 0, 255}}},
	{NULL}
};

osc_control* osc_controls = NULL;

// THIS IS CURRENTLY PRETTY HACKY AND NOT USER FRIENDLY AT ALL

float osc_param_float(uint8_t* buffer, unsigned index){
	unsigned u;
	union{
		float rv;
		uint8_t in[4];
	} conv;

	for(u = 0; u < 4; u++){
		conv.in[3 - u] = buffer[(index * 4) + u];
	}

	return conv.rv;
}

#define nextdword(a) ((((a) / 4) + (((a) % 4) ? 1:0)) * 4)
int osc_parse(char* buffer, size_t len, char** path, unsigned* num_args, uint8_t** args){
	unsigned args_supplied = 0;
	char* param_str;
	char* data_off;

	param_str = buffer + nextdword(strlen(buffer) + 1);
	data_off = param_str + nextdword(strlen(param_str) + 1);

	if(param_str + strlen(param_str) > (buffer + len) || data_off + (4 * args_supplied) > (buffer + len)){
		fprintf(stderr, "OSC message out of bounds\n");
		return -1;
	}

	if(*param_str != ','){
		fprintf(stderr, "Invalid OSC format string %s (offset %zu)\n", param_str, param_str - buffer);
		return -1;
	}

	args_supplied = strlen(param_str + 1);

	if(path){
		*path = buffer;
	}

	if(num_args){
		*num_args = args_supplied;
	}

	if(args){
		*args = (uint8_t*)data_off;
	}
	return 0;
}

int input_negotiate(int fd, char* devname, char* password){
	char msg_buf[MSG_MAX * 4];
	ssize_t bytes, bytes_xfered;
	char* offset;

	bytes = snprintf(msg_buf, sizeof(msg_buf), "HELLO %s\nDEVTYPE 2\nNAME %s\nPASSWORD %s\n\n", PROTOCOL_VERSION, devname, password);
	//ABS_X MIN 0
	//ABS_X MAX 255
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
	while(memchr(msg_buf, '\n', bytes) == NULL);

	//terminate response
	offset = memchr(msg_buf, '\n', bytes);
	*offset = 0;

	if(strtoul(msg_buf, NULL, 10) != 200){
		fprintf(stderr, "Server reported: %s\n", msg_buf);
		return -1;
	}

	fprintf(stderr, "Reconnection token (ignored): %s\n", msg_buf + 4);
	return 0;
}

int configure_mappings(int osc_fd){
	gp_control* current_control = gamepad_controls;
	ssize_t bytes;
	size_t num_osc = 0, u;
	fd_set read_fds;
	int status;
	char buffer[INPUT_BUFFER_SIZE];
	double value;

	char* path;
	unsigned num_args;
	uint8_t* args;

	osc_control template = {
		.path = NULL,
		.num_channels = 0,
		.channels = {
		}
	};

	osc_control current_osc;

	while(current_control->name){
		fprintf(stderr, "Now configuring control %s\nPlease press the button or move the axes to all extrema\nPress enter to continue, s to skip or q to quit\n", current_control->name);
		current_osc = template;
		current_osc.num_channels = current_control->num_channels;
		current_osc.type = current_control->type;
		for(u = 0; u < current_osc.num_channels; u++){
			current_osc.channels[u].code = current_control->channels[u].code;
		}

		//fetch events
		while(!shutdown_requested){
			FD_ZERO(&read_fds);
			FD_SET(osc_fd, &read_fds);
			FD_SET(fileno(stdin), &read_fds);
			status = select(((fileno(stdin) > osc_fd) ? fileno(stdin):osc_fd) + 1, &read_fds, NULL, NULL, NULL);
			if(status < 0){
				perror("config/select");
				return -1;
			}
			else{
				if(FD_ISSET(fileno(stdin), &read_fds)){
					//handle keyboard input
					read(fileno(stdin), buffer, sizeof(buffer));
					break;
				}

				if(FD_ISSET(osc_fd, &read_fds)){
					//read osc message and set limits
					bytes = recv(osc_fd, buffer, sizeof(buffer), 0);
					if(bytes <= 0){
						perror("config/recv");
						return -1;
					}

					//parse osc message into buffer
					if(osc_parse(buffer, bytes, &path, &num_args, &args) < 0){
						fprintf(stderr, "Failed to parse OSC data\n");
						return -1;
					}

					if(num_args != current_osc.num_channels){
						fprintf(stderr, "Ignoring packet with different number of channels\n");
						continue;
					}

					if(!current_osc.path){
						current_osc.path = strdup(path);
					}
					fprintf(stderr, ".");
					fflush(stderr);

					for(u = 0; u < current_osc.num_channels; u++){
						value = osc_param_float(args, u);
						current_osc.channels[u].min = (value < current_osc.channels[u].min) ? value:current_osc.channels[u].min;
						current_osc.channels[u].max = (value > current_osc.channels[u].max) ? value:current_osc.channels[u].max;
					}
				}
			}
		}

		if(*buffer == 'q'){
			return -1;
		}

		if(*buffer != 's'){
			//show and confirm config
			if(*buffer == '\n'){
				if(!current_osc.path){
					fprintf(stderr, "No OSC path could be found, please try again\n");
					continue;
				}

				fprintf(stderr, "Control OSC Path: %s\n", current_osc.path);
				for(u = 0; u < current_osc.num_channels; u++){
					fprintf(stderr, "Channel %zu min %f max %f\n", u, current_osc.channels[u].min, current_osc.channels[u].max);
				}

				//TODO get confirmation, else again
				
				//create osc control
				osc_controls = realloc(osc_controls, (num_osc + 2) * sizeof(osc_control));
				if(!osc_controls){
					fprintf(stderr, "Failed to allocate memory\n");
					return -1;
				}
				osc_controls[num_osc] = current_osc;
				osc_controls[num_osc + 1] = template;
				num_osc++;
			}
		}
		current_control++;

	}

	fprintf(stderr, "All controls configured\n");
	return 0;
}

void signal_handler(int param){
	shutdown_requested = 1;
}

int osc_msg_xlate(int osc_fd, int host_fd){
	struct input_event event;
	char buffer[INPUT_BUFFER_SIZE];
	ssize_t bytes;
	size_t u, c;

	char* path;
	uint8_t* data;
	unsigned num_args;

	bytes = recv(osc_fd, buffer, sizeof(buffer), 0);
	if(bytes <= 0){
		perror("xlate/recv");
		return -1;
	}

	if(osc_parse(buffer, bytes, &path, &num_args, &data) < 0){
		fprintf(stderr, "Invalid OSC packet\n");
		return -1;
	}

	for(u = 0; osc_controls && osc_controls[u].path; u++){
		if(!strcmp(osc_controls[u].path, path)){
			//event type
			event.type = osc_controls[u].type; 
			for(c = 0; c < osc_controls[u].num_channels; c++){
				event.code = osc_controls[u].channels[c].code;
				//send event
				//FIXME might want to apply internal scaling here
				event.value = (int)roundf(osc_param_float(data, c));
				fprintf(stderr, "Event type:%d, code:%d, value:%d (raw %f max %f)\n", event.type, event.code, event.value, osc_param_float(data, c), osc_controls[u].channels[c].max);
				send(host_fd, &event, sizeof(struct input_event), 0);
			}

			//send syn
			event.type = EV_SYN;
			event.code = SYN_REPORT;
			event.value = 0;
			send(host_fd, &event, sizeof(struct input_event), 0);
			return 0;
		}
	}

	fprintf(stderr, "Unknown OSC path\n");
	return 0;
}

int main(int argc, char** argv){
	unsigned u;
	//ext conf
	//name
	char* osc_port = getenv("OSC_PORT") ? getenv("OSC_PORT"):DEFAULT_OSC_PORT;
	char* osc_host = getenv("OSC_HOST") ? getenv("OSC_HOST"):DEFAULT_OSC_HOST;
	char* device_name = getenv("SERVER_NAME") ? getenv("SERVER_NAME"):"OSC Gamepad Bridge";
	char* input_host = getenv("SERVER_HOST") ? getenv("SERVER_HOST"):DEFAULT_HOST;
	char* input_port = getenv("SERVER_PORT") ? getenv("SERVER_PORT"):DEFAULT_PORT;
	char* password = getenv("SERVER_PW") ? getenv("SERVER_PW"):DEFAULT_PASSWORD;

	int osc_fd, host_fd;

	signal(SIGINT, signal_handler);
	fprintf(stderr, "%s\n", PROGRAM_NAME);

	//set stdin nonblocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	if(fcntl(fileno(stdin), F_SETFL, flags) < 0){
		perror("fcntl");
		return EXIT_FAILURE;
	}

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
	if(configure_mappings(osc_fd) < 0){
		fprintf(stderr, "Failed to configure OSC mappings\n");
		return EXIT_FAILURE;
	}

	//xlate events
	while(!shutdown_requested){
		if(osc_msg_xlate(osc_fd, host_fd) < 0){
			fprintf(stderr, "Translation failed\n");
			break;
		}
	}

	//free mappings
	for(u = 0; osc_controls && osc_controls[u].path; u++){
		free(osc_controls[u].path);
	}
	free(osc_controls);

	fprintf(stderr, "Shutting down\n");
	close(osc_fd);
	close(host_fd);
	return EXIT_SUCCESS;
}

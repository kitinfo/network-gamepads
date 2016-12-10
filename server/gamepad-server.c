#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/random.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include "../libs/logger.h"
#include <signal.h>
#include <stdlib.h>

#include "sockfd.c"

int MAX_DEVICES = 8;
volatile int shutdown_server = false;
const unsigned MAX_SIZE = 256;
const char* VERSION = "1.0";
const unsigned MAX_TOKEN = 64;

struct libevdev* create_new_node(const char* identifier) {
	struct libevdev* dev = libevdev_new();
	libevdev_set_uniq(dev, identifier);
	libevdev_set_id_version(dev, 0x114);
	libevdev_set_id_vendor(dev, 0x45e);
	libevdev_set_id_bustype(dev, 0x3);
	libevdev_set_id_product(dev, 0x28e);
	//libevdev_set_name(dev, "Gamepad-Server Virtual Device");
	libevdev_set_name(dev, "Microsoft X-Box 360 pad");
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_type(dev, EV_ABS);
	libevdev_enable_event_type(dev, EV_REL);

	int i;
	for (i = 0; i < 256; i++) {
		libevdev_enable_event_code(dev, EV_KEY, i, NULL);
	}
	// syn types
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_code(dev, EV_SYN, SYN_MT_REPORT, NULL);
	libevdev_enable_event_code(dev, EV_SYN, SYN_DROPPED, NULL);
	libevdev_enable_event_code(dev, EV_SYN, SYN_REPORT, NULL);

	// buttons
	libevdev_enable_event_code(dev, EV_KEY, BTN_A, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_B, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_X, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_Y, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TL, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TR, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_SELECT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_START, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMBL, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMBR, NULL);
	
	
	libevdev_enable_event_code(dev, EV_KEY, BTN_MOUSE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);


	// rel
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Z, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RX, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RY, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RZ, NULL);

	// hacky absinfo from xbox controller
	struct input_absinfo absinfo = {
		.value = -2866,
		.minimum = -32768,
		.maximum = 32767,
		.fuzz = 16,
		.flat = 128
	};

	// abs
	libevdev_enable_event_code(dev, EV_ABS, ABS_X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_Z, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RX, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RY, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RZ, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT0X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT0Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT1X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT1Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT2X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT2Y, &absinfo);

	return dev;
}

// TODO: add check for getrandom function
#ifndef getrandom

int getrandom(char* rand, unsigned size, unsigned int flags) {
	FILE* fd = fopen("/dev/urandom", "r");
	if (!fd) {
		perror("cannot open /dev/urandom");
		return -1;
	}
	fread(rand, 1, size, fd);
	fclose(fd);
	return 0;
}

#endif

int genId(char* id, unsigned size) {
	unsigned offset = 0;
	memset(id, 0, size);
	char rand[size / 2];
	memset(rand, 0, (size /2));

	int status = getrandom(rand, (unsigned) (size / 2), 0);
	unsigned i;
	for (i = 0; i < (unsigned) (size / 2); i++) {
		if (offset + 2 < size) {
			snprintf(id + offset, 3, "%02X", rand[i]);
			offset += 2;
		}
	}

	id[size - 1] = 0;

	return status;
}

int check_version(int fd, char* version) {
	if (!strncmp(version, VERSION, strlen(VERSION))) {
		return 0;
	} else {
		int len = snprintf(NULL, 0, "400 Version not matched (Server: %s, Client: %s)", VERSION, version);
		char out[len + 1];
		snprintf(out, len + 1, "400 Version not matched (Server: %s, Client: %s)", VERSION, version);
		return -1;
	}
}

int check_hello(int fd, char* msg, char* token, char* password) {

	char* pw = strstr(msg, " ");
	if (pw == NULL) {
		sock_send(fd, "405 No Password supplied");
		return -1;
	}

	unsigned pw_len = strlen(pw);

	if (pw_len < 1) {
		sock_send(fd, "405 No Password supplied");
		return -1;
	}
	pw[0] = 0;
	pw++;

	if (check_version(fd, msg) < 0) {
		return -1;
	}

	if (strncmp(pw, password, strlen(password))) {
		sock_send(fd, "401 Wrong password");
		return -1;
	}

	//if (genId(token, MAX_TOKEN) < 0) {
	//	return -1;
	//}

	char out[strlen(token) + 5];
	snprintf(out, sizeof(out), "%s %s", "200", token);
	if (sock_send(fd, out) < 0) {
		perror("check_hello/send");
		return -1;
	}

	return 0;
}

int check_continue(int fd, char* msg, char* token) {

	char* t = strstr(msg, " ");

	if (t == NULL) {
		sock_send(fd, "406 No Token supplied");
		return -1;
	}

	unsigned t_len = strlen(t);

	if (t_len < 1) {
		sock_send(fd, "406 No Token supplied");
		return -1;
	}

	t[0] = 0;
	t++;

	if (check_version(fd, msg) < 0) {
		return -1;
	}

	if (strncmp(t, token, strlen(token))) {
		sock_send(fd, "403 Token expired");
		return -1;
	}

	char out[strlen(token) + 5];
	snprintf(out, sizeof(out), "%s %s", "200", token);

	sock_send(fd, out);
	return 0;
}

int check_connect(int fd, char* token, char* password) {

	char msg[MAX_SIZE + 1];
	memset(msg, 0, MAX_SIZE + 1);

	int bytes = recv(fd, msg, MAX_SIZE, 0);

	if (bytes < 0) {
		perror("check_connect/recv");
		return -1;
	}
	printf("msg: %s\n", msg);
	if (!strncmp(msg, "HELLO", 5)) {
		return check_hello(fd, msg + 6, token, password);
	} else if (!strncmp(msg, "CONTINUE", 8)) {
		return check_continue(fd, msg + 9, token);
	}
	printf("Unkown command\n");
	int bytes_send = 0;
	char* ans = "400 Unkown Command\n";
	unsigned ans_len = strlen(ans);
	do {
		bytes_send = send(fd, ans, ans_len - bytes_send, 0);
		ans_len -= bytes_send;
	} while (ans_len > 0);
	return 0;
}

int run_server(struct libevdev_uinput* uidev, int uinput_fd, int sock_fd, char* token, char* password) {

	if (sock_fd < 0) {
		return -1;
	}

	int accept_fd;
	do {
		accept_fd = accept(sock_fd, NULL, NULL);
		printf("Found client.\n");
	} while(check_connect(accept_fd, token, password) < 0 && shutdown_server);


	char buf[256];
	memset(buf, 0, 256);
	int bytes = 0;
	struct input_event ev;

	while (!shutdown_server) {
		bytes = recv(accept_fd, &ev, sizeof(struct input_event), 0);

		if (bytes <= 0) {
			perror("run_server/read");
			break;
		}

		printf("bytes recv: %d\n", bytes);
		printf("ev.type: %d, ev.code %d, ev.value %d\n", ev.type, ev.code, ev.value);
		//bytes = write(uinput_fd, buf, bytes);
		libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);

		if (bytes <= 0) {
			perror("run_server/write");
		}
	}
	printf("Shutting down...\n");
	return 0;
}

void s_shutdown(int param) {
	shutdown_server = true;
}

int main(int argc, char** argv) {

	// init logger
	LOGGER log = {
		.stream = stderr,
		.verbosity = 5
	};

	char* password = getenv("GAMEPAD_SERVER_PW");

	if (password == NULL) {
		password = "0000";
	}

	logprintf(log, LOG_INFO, "Password is %s\n", password);

	char* port_s = getenv("GAMEPAD_SERVER_PORT");
	unsigned port = 0;

	if (port_s != NULL) {
		port = strtoul(port_s, NULL, 10);
		logprintf(log, LOG_INFO, "Port is %d\n", port);
	}


	logprintf(log, LOG_INFO, "Starting server...\n");

	char id[MAX_TOKEN + 1];
	if (genId(id, MAX_TOKEN) < 0) {
		return 100;
	}
	logprintf(log, LOG_INFO, "generated id: %s\n", id);

	//char* id = "123456789ABCDEF";
	struct libevdev* dev = create_new_node(id);
	struct libevdev_uinput* uidev;
	if (libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev) != 0) {
		return 110;
	}
	signal(SIGINT, s_shutdown);
	logprintf(log, LOG_INFO, "input device: %s\n", libevdev_uinput_get_devnode(uidev));
	int uinput_fd = libevdev_uinput_get_fd(uidev);

	int sock_fd = sock_open("*", port);
	if (sock_fd < 0) {
		return -1;
	}
	while (!shutdown_server) {
		run_server(uidev, uinput_fd, sock_fd, id, password);
	}
	sock_close(sock_fd);
	libevdev_uinput_destroy(uidev);
	libevdev_free(dev);
	return 0;
}

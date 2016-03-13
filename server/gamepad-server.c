#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/random.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include "../libs/logger.h"
#include <signal.h>

#include "sockfd.c"

int MAX_DEVICES = 8;
volatile int shutdown_server = false;

struct libevdev* create_new_node(const char* identifier) {
	struct libevdev* dev = libevdev_new();
	libevdev_set_uniq(dev, identifier);
	libevdev_set_name(dev, "Gamepad-Server Virtual Device");
	libevdev_enable_event_type(dev, EV_KEY);
	/*
	libevdev_enable_event_code(dev, EV_KEY, KEY_D, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_ENTER, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_NUMLOCK, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_MINUS, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_BACKSPACE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_COMMA, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_NUMLOCK, NULL);
	libevdev_enable_event_code(dev, EV_KEY, KEY_EQUAL, NULL);
	libevdev_enable_event_code(dev, EV_KEY, 29, NULL);
	*/
	int i;
	for (i = 0; i < 128; i++) {
		libevdev_enable_event_code(dev, EV_KEY, i, NULL);
	}
	return dev;
}

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

int run_server(struct libevdev_uinput* uidev, int uinput_fd, char* host, int port) {
	int sock_fd = sock_open(host, port);

	if (sock_fd < 0) {
		return -1;
	}

	int accept_fd = accept(sock_fd, NULL, NULL);

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
	sock_close(sock_fd);
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

	logprintf(log, LOG_INFO, "Starting server...\n");
/*
	char id[64];
	if (genId(id, 64) < 0) {
		return 100;
	}
	logprintf(log, LOG_INFO, "generated id: %s\n", id);
*/
	char* id = "123456789ABCDEF";
	struct libevdev* dev = create_new_node(id);
	struct libevdev_uinput* uidev;
	if (libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev) != 0) {
		return 110;
	}
	signal(SIGINT, s_shutdown);
	logprintf(log, LOG_INFO, "input device: %s\n", libevdev_uinput_get_devnode(uidev));
	int uinput_fd = libevdev_uinput_get_fd(uidev);

	run_server(uidev, uinput_fd, "localhost", 7999);

	libevdev_uinput_destroy(uidev);
	libevdev_free(dev);
	return 0;
}

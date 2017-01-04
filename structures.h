#pragma once
#include <linux/input.h>

#include "protocol.h"

enum DEV_TYPE {
	DEV_TYPE_UNKOWN = 0,
	DEV_TYPE_MICE = 1,
	DEV_TYPE_GAMEPAD = 2,
	DEV_TYPE_KEYBOARD = 3
};

struct device_meta {
	int devtype;
	char* name;
	struct input_id id;
};

typedef struct /*_GAMEPAD_CLIENT*/ {
	int fd;
	int ev_fd;
	bool passthru;
	size_t scan_offset;
	char token[TOKEN_SIZE + 1];
	uint8_t input_buffer[INPUT_BUFFER_SIZE];
} gamepad_client;

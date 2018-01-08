#pragma once
#include <inttypes.h>
#include <linux/input.h>

#include "protocol.h"

struct device_meta {
	int devtype;
	char* name;
	struct input_id id;
	struct input_absinfo absinfo[ABS_CNT];
};

typedef struct /*_GAMEPAD_CLIENT*/ {
	int fd;
	int ev_fd;
	struct device_meta meta;
	uint8_t last_ret;
	size_t scan_offset;
	uint8_t input_buffer[INPUT_BUFFER_SIZE];
	ssize_t bytes_available;
} gamepad_client;

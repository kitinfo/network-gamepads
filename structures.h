#pragma once
#include <linux/input.h>

#include "protocol.h"

struct device_meta {
	int devtype;
	char* name;
	struct input_id id;
	__s32 absmax[ABS_CNT];
	__s32 absmin[ABS_CNT];
	__s32 absfuzz[ABS_CNT];
	__s32 absflat[ABS_CNT];
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

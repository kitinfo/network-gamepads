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
	__s32 absmax[ABS_CNT];
	__s32 absmin[ABS_CNT];
	__s32 absfuzz[ABS_CNT];
	__s32 absflat[ABS_CNT];
};

typedef struct /*_GAMEPAD_CLIENT*/ {
	int fd;
	int ev_fd;
	bool passthru;
	size_t scan_offset;
	char token[TOKEN_SIZE + 1];
	uint8_t input_buffer[INPUT_BUFFER_SIZE];
} gamepad_client;

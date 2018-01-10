#pragma once

#include <inttypes.h>
#include <linux/input.h>

#include "../common/protocol.h"

#include "../libs/logger.h"

struct enabled_event {
	unsigned long type;
	unsigned long code;
};

struct device_meta {
	char* name;
	size_t enabled_events_length;
	struct enabled_event* enabled_events;
	struct input_id id;
	struct input_absinfo absinfo[ABS_CNT];
};

typedef struct /*_GAMEPAD_CLIENT*/ {
	int fd;
	int ev_fd;
	struct device_meta meta;
	uint8_t status;
	size_t scan_offset;
	uint8_t input_buffer[INPUT_BUFFER_SIZE];
	ssize_t bytes_available;
} gamepad_client;

typedef struct {
	LOGGER log;
	char* program_name;
	char* bindhost;
	char* port;
	char* password;
} Config;

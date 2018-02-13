#pragma once
#include "../libs/logger.h"

#define VERSION "InputClient 2.0"

typedef struct {
	LOGGER log;
	char* program_name;
	char* dev_name;
	char* dev_path;
	char* password;
	char* host;
	char* port;
	uint64_t type;
	uint8_t slot;
	int reopen_attempts;
} Config;

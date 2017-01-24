#pragma once
#include "libs/logger.h"

typedef struct {
	LOGGER log;
	char* program_name;
	char* password;
	char* host;
	char* port;
	uint64_t type;
	uint8_t slot;
} Config;

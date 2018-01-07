#pragma once

#include <inttypes.h>
#include "../libs/logger.h"

typedef struct {
	LOGGER log;
	uint64_t limit;
	char* program_name;
	char* bindhost;
	char* port;
	char* password;
} Config;

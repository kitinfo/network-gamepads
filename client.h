#pragma once
#include "libs/logger.h"

typedef struct {
	LOGGER log;
	char* program_name;
	char* password;
	char* host;
	char* port;
	int type;
} Config;

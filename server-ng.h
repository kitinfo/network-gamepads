#pragma once

typedef struct {
	LOGGER log;
	uint64_t limit;
	char* program_name;
	char* bindhost;
	char* port;
	char* password;
} Config;

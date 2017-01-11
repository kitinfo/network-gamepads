#pragma once

typedef struct {
	LOGGER log;
	char* program_name;
	char* bindhost;
	char* port;
	char* password;
} Config;

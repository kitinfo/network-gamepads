#pragma once

typedef struct {
	LOGGER log;
	enum DEV_TYPE limit;
	char* program_name;
	char* bindhost;
	char* port;
	char* password;
} Config;

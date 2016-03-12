#include <stdio.h>

#include "../libs/logger.h"

int main(int argc, char** argv) {

	// init logger
	LOGGER log = {
		.stream = stdin,
		.verbosity = 3
	};

	logprintf(log, LOG_INFO, "Starting server...\n");

	return 0;
}

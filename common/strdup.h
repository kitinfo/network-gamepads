#pragma once

#include <string.h>
#include <features.h>

#if !(_XOPEN_SOURCE >= 500 \
    || /* Since glibc 2.12: */ _POSIX_C_SOURCE >= 200809L \
	|| /* Glibc versions >= 2.19: */ _BSD_SOURCE || _SVID_SOURCE)

	char* strdup(const char *s) {
		unsigned len = strlen(s) + 1;
		char* n = calloc(len, sizeof(char));
		strncpy(n, s, len);
		return n;
	}

#endif

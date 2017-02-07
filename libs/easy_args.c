#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "easy_args.h"

typedef enum ARG_TYPE {
	TYPE_INT,
	TYPE_UINT,
	TYPE_FUNC,
	TYPE_FLAG,
	TYPE_STRING
} ARG_TYPE;

struct ArgumentItem {

	char* argShort;
	char* argLong;
	ARG_TYPE type;
	void* func;
	unsigned arguments;
	struct ArgumentItem* next;
};

struct ArgumentItem* base;

int eargs_addArgumentElem(char* argShort, char* argLong, void* func, unsigned arguments, ARG_TYPE type) {

	// init struct and fill with arguments
	struct ArgumentItem* item = (struct ArgumentItem*) malloc(sizeof(struct ArgumentItem));
	item->argShort = argShort;
	item->argLong = argLong;
	item->func = func;
	item->arguments = arguments;
	item->type = type;
	item->next = NULL;

	// check if base is initialized.
	if (!base) {
		base = item;
	} else {
		// find last item in argument list
		struct ArgumentItem* next = base;
		while (next->next) {
			next = next->next;
		}

		// add new argument
		next->next = item;
	}

	return 1;
}

int eargs_addArgumentString(char* argShort, char* argLong, char** container) {
	return eargs_addArgumentElem(argShort, argLong, (void*) container, 1, TYPE_STRING);
}

int eargs_addArgumentFlag(char* argShort, char* argLong, bool* container) {

	return eargs_addArgumentElem(argShort, argLong, (void*) container, 0, TYPE_FLAG);
}

int eargs_addArgumentInt(char* argShort, char* argLong, int* container) {

	return eargs_addArgumentElem(argShort, argLong, (void*) container, 1, TYPE_INT);
}

int eargs_addArgument(char* argShort, char* argLong, void* func, unsigned arguments) {
	return eargs_addArgumentElem(argShort, argLong, func, arguments, TYPE_FUNC);
}

int eargs_addArgumentUInt(char* argShort, char* argLong, unsigned* container) {
	return eargs_addArgumentElem(argShort, argLong, (void*) container, 1, TYPE_UINT);
}

int eargs_clearItem(struct ArgumentItem* item) {
	// free when last item else recursive
	if (item->next) {
		eargs_clearItem(item->next);
	}

	free(item);

	return 1;
}

int eargs_clear() {

	// check if base is initialized.
	if (base) {
		// begin clearing
		eargs_clearItem(base);
	}

	return 1;
}

int eargs_handle_string(struct ArgumentItem* item, int argc, char** cmds, void* config) {
	char** container = (char**) item->func;
	*container = cmds[1];

	return 1;
}

int eargs_handle_flag(struct ArgumentItem* item, int argc, char** cmds, void* config) {
	bool* container = (bool*) item->func;
	*container = true;

	return 0;
}

int eargs_handle_int(struct ArgumentItem* item, int argc, char** cmds, void* config) {
	int* container = (int*) item->func;
	*container = strtol(cmds[1], NULL, 10);

	return 1;
}

int eargs_handle_uint(struct ArgumentItem* item, int argc, char** cmds, void* config) {
	unsigned* container = (unsigned*) item->func;
	*container = strtoul(cmds[1], NULL, 10);

	return 1;
}

int eargs_handle_func(struct ArgumentItem* item, int argc, char** cmds, void* config) {
	// call function
	int (*p)(int argc, char** argv, void* config) = item->func;
	return p(argc, cmds, config);
}

int eargs_action(struct ArgumentItem* item, int argc, char** cmds, void* config) {

	switch(item->type) {
		case TYPE_INT:
			return eargs_handle_int(item, argc, cmds, config);
		case TYPE_UINT:
			return eargs_handle_uint(item, argc, cmds, config);
		case TYPE_FUNC:
			return eargs_handle_func(item, argc, cmds, config);
		case TYPE_FLAG:
			return eargs_handle_flag(item, argc, cmds, config);
		case TYPE_STRING:
			return eargs_handle_string(item, argc, cmds, config);
		default:
			printf("type not implemented.\n");
			return -1;
	}
}

int eargs_parseItem(int argc, char** cmds, void* config) {

	struct ArgumentItem* item = base;
	int arg = -1;

	// no args
	if (argc < 1) {
		return arg;
	}

	while (item) {

		// check if argShort matches or if argLong matches (NULL will be excluded)
		if ((item->argShort && !strcmp(cmds[0], item->argShort)) || (item->argLong && !strcmp(cmds[0], item->argLong))) {
			// check if enough arguments are available
			if (argc > item->arguments) {
				arg = eargs_action(item, argc, cmds, config);
				if (arg < 0) {
					return -2;
				} else {
					return arg;
				}
			} else {
				printf("(%s,%s) needs an argument.", item->argShort,item->argLong);
				return -2;
			}
		}
		item = item->next;
	}

	return arg;
}

// output should be initialized with: argc * sizeof(char*))
int eargs_parse(int argc, char** argv, char** output, void* config) {

	if (output) {
		// memset output array (don't trust);
		memset(output, 0, argc * sizeof(char*));
	}
	int outputc = 0;
	int i;

	for (i = 1; i < argc; i++) {
		int v = eargs_parseItem(argc - i, &argv[i], config);

		// -2 means error in parsing the argument
		if (v == -2) {
			return -2;
		// -1 means no identifier found for this argument -> add to output list
		} else if (v < 0) {
			if (output) {
				output[outputc] = argv[i];
			}
			outputc++;
		} else {
			// skip arguments used by identifier.
			i += v;
		}
	}

	// clear struct
	eargs_clear();

	return outputc;
}

#pragma once

#include <linux/input.h>

#define PROTOCOL_VERSION 0x02
#define INPUT_BUFFER_SIZE 1024
#define DEFAULT_PASSWORD "foobar"
#define DEFAULT_HOST "::"
#define DEFAULT_PORT "9292"

enum DEV_TYPE {
	DEV_TYPE_UNKNOWN = 0,
	DEV_TYPE_MOUSE = 1,
	DEV_TYPE_GAMEPAD = 2,
	DEV_TYPE_KEYBOARD = 3
};

enum MESSAGE_TYPES {
	MESSAGE_HELLO = 0x01,
	MESSAGE_PASSWORD = 0x02,
	MESSAGE_ABSINFO = 0x03,
	MESSAGE_DEVICE = 0x04,
	MESSAGE_SETUP_END = 0x05,
	MESSAGE_DATA = 0x10,
	MESSAGE_SUCCESS = 0xF0,
	MESSAGE_VERSION_MISMATCH = 0xF1,
	MESSAGE_INVALID_PASSWORD = 0xF2,
	MESSAGE_INVALID_CLIENT_SLOT = 0xF3,
	MESSAGE_INVALID = 0xF4,
	MESSAGE_PASSWORD_REQUIRED = 0xF5,
	MESSAGE_SETUP_REQUIRED = 0xF6,
	MESSAGE_CLIENT_SLOT_IN_USE = 0xF7,
	MESSAGE_CLIENT_SLOTS_EXHAUSTED = 0xF8,
	MESSAGE_QUIT = 0xF9,
	MESSAGE_DEVICE_NOT_ALLOWED = 0xFA
};

struct MessageInfo {
	unsigned length;
	char* name;
};
typedef struct {
	uint8_t msg_type;
	uint8_t version;
	uint8_t slot;
} HelloMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t length;
	char password[];
} PasswordMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t axis;
	struct input_absinfo info;
} ABSInfoMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t length;
	uint8_t type;
	struct input_id ids;
	char name[];
} DeviceMessage;

typedef struct {
	uint8_t msg_type;
	struct input_event event;
} DataMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t version;
} VersionMismatchMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t slot;
} SuccessMessage;

struct MessageInfo MESSAGE_TYPES_INFO[256] = {
	[0 ... 255] = { .length = -1, .name = "unkown"},
	[MESSAGE_HELLO] = { .length = sizeof(HelloMessage), .name = "hello"},
	[MESSAGE_PASSWORD] = { .length = sizeof(PasswordMessage), .name = "password"},
	[MESSAGE_ABSINFO] = {.length = sizeof(ABSInfoMessage), .name = "absinfo"},
	[MESSAGE_DEVICE] = { .length = sizeof(DeviceMessage), .name = "device"},
	[MESSAGE_SETUP_END] = { .length = 1, .name = "setup end"},
	[MESSAGE_DATA] =  { .length = sizeof(DataMessage), .name = "data"},
	[MESSAGE_SUCCESS] = { .length = sizeof(SuccessMessage), .name = "success"},
	[MESSAGE_VERSION_MISMATCH] = { .length = sizeof(VersionMismatchMessage), .name = "version mismatch"},
	[MESSAGE_INVALID_PASSWORD] = { .length = 1, .name = "invalid password"},
	[MESSAGE_INVALID_CLIENT_SLOT] = { .length = 1, .name = "invalid client slot"},
	[MESSAGE_INVALID] = { .length = 1, .name = "invalid message"},
	[MESSAGE_PASSWORD_REQUIRED] = { .length = 1, .name = "password required"},
	[MESSAGE_SETUP_REQUIRED] = { .length = 1, .name = "setup required"},
	[MESSAGE_CLIENT_SLOT_IN_USE] = { .length = 1, .name = "client slot in use"},
	[MESSAGE_CLIENT_SLOTS_EXHAUSTED] = { .length = 1, .name = "client slots exhausted"},
	[MESSAGE_QUIT] = { .length = 1, .name = "quit"},
	[MESSAGE_DEVICE_NOT_ALLOWED] = { .length = 1, .name = "device is not allowed"}
};


char* get_message_name(uint8_t msg) {
	return MESSAGE_TYPES_INFO[msg].name;
}

int get_size_from_command(uint8_t* buf, unsigned len) {
	if (buf[0] == MESSAGE_PASSWORD || buf[0] == MESSAGE_DEVICE) {
		if (len > 1) {
			return MESSAGE_TYPES_INFO[buf[0]].length + buf[1];
		} else {
			return 0;
		}
	} else {
		return MESSAGE_TYPES_INFO[buf[0]].length;
	}
}

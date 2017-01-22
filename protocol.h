#pragma once

#include <linux/input.h>

#define PROTOCOL_VERSION 0x01
#define INPUT_BUFFER_SIZE 1024
#define DEFAULT_PASSWORD "foobar"
#define DEFAULT_HOST "::"
#define DEFAULT_PORT "9292"

enum MESSAGE_TYPES {
	MESSAGE_HELLO = 0x01,
	MESSAGE_PASSWORD = 0x02,
	MESSAGE_ABSINFO = 0x03,
	MESSAGE_DEVICE = 0x04,
	MESSAGE_SETUP_END = 0x05,
	MESSAGE_DATA =  0x10,
	MESSAGE_SUCCESS = 0xF0,
	MESSAGE_VERSION_MISMATCH = 0xF1,
	MESSAGE_INVALID_PASSWORD = 0xF2,
	MESSAGE_INVALID_CLIENT_SLOT = 0xF3,
	MESSAGE_INVALID = 0xF4,
	MESSAGE_PASSWORD_REQUIRED = 0xF5,
	MESSAGE_SETUP_REQUIRED = 0xF6,
	MESSAGE_CLIENT_SLOT_IN_USE = 0xF7,
	MESSAGE_CLIENT_SLOTS_EXHAUSTED = 0xF8,
	MESSAGE_QUIT = 0xF9
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

char* get_message_name(uint8_t msg) {
	switch(msg) {
		case MESSAGE_HELLO:
			return "hello";
		case MESSAGE_PASSWORD:
			return "password";
		case MESSAGE_ABSINFO:
			return "absinfo";
		case MESSAGE_DEVICE:
			return "device";
		case MESSAGE_SETUP_END:
			return "setup end";
		case MESSAGE_DATA:
			return "data";
		case MESSAGE_SUCCESS:
			return "success";
		case MESSAGE_VERSION_MISMATCH:
			return "version mismatch";
		case MESSAGE_INVALID_PASSWORD:
			return "password";
		case MESSAGE_INVALID_CLIENT_SLOT:
			return "invalid client slot";
		case MESSAGE_INVALID:
			return "invalid message";
		case MESSAGE_PASSWORD_REQUIRED:
			return "password required";
		case MESSAGE_SETUP_REQUIRED:
			return "setup required";
		case MESSAGE_CLIENT_SLOT_IN_USE:
			return "client slot in use";
		case MESSAGE_CLIENT_SLOTS_EXHAUSTED:
			return "client slots exhausted";
		case MESSAGE_QUIT:
			return "quit";
		default:
			return "unkown";
	}
}

int get_size_from_command(uint8_t* buf, unsigned len) {
	switch(buf[0]) {
		case MESSAGE_HELLO:
			return sizeof(HelloMessage);
		case MESSAGE_PASSWORD:
			if (len > 1) {
				// 2 byte for command and length + length of the password
				return sizeof(PasswordMessage) + buf[1];
			} else {
				return 0;
			}
		case MESSAGE_ABSINFO:
			return sizeof(ABSInfoMessage);
		case MESSAGE_DEVICE:
			if (len > 1) {
				// 3 bytes for command, length and type + sizeof(struct input_id) + length of the name
				return sizeof(DeviceMessage) + buf[1];
			} else {
				return 0;
			}
		case MESSAGE_SUCCESS:
			return sizeof(SuccessMessage);
		case MESSAGE_VERSION_MISMATCH:
			return sizeof(VersionMismatchMessage);
		case MESSAGE_SETUP_END:
		case MESSAGE_INVALID_PASSWORD:
		case MESSAGE_INVALID_CLIENT_SLOT:
		case MESSAGE_INVALID:
		case MESSAGE_PASSWORD_REQUIRED:
		case MESSAGE_SETUP_REQUIRED:
		case MESSAGE_CLIENT_SLOT_IN_USE:
		case MESSAGE_CLIENT_SLOTS_EXHAUSTED:
		case MESSAGE_QUIT:
			return 1;
		case MESSAGE_DATA:
			return sizeof(DataMessage);
		default:
			return -1;
	}
}

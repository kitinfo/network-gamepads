#pragma once

#include <linux/input.h>

#define PROTOCOL_VERSION "2.2"
#define BINARY_PROTOCOL_VERSION 0x01
#define INPUT_BUFFER_SIZE 1024
#define DEFAULT_PASSWORD "foobar"
#define DEFAULT_HOST "::"
#define DEFAULT_PORT "9292"
#define TOKEN_SIZE 16
#define MAX_STATUS_MESSAGE_SIZE 128

enum MESSAGE_TYPES {
	MESSAGE_HELLO = 0x01,
	MESSAGE_PASSWORD = 0x02,
	MESSAGE_ABSINFO = 0x03,
	MESSAGE_DEVICE = 0x04,
	MESSAGE_HELLO_END = 0x05,
	MESSAGE_DATA =  0x10,
	MESSAGE_SUCCESS = 0xF0,
	MESSAGE_VERSION_MISMATCH = 0xF1,
	MESSAGE_INVALID_PASSWORD = 0xF2,
	MESSAGE_INVALID_CLIENT_SLOT = 0xF3,
	MESSAGE_INVALID_COMMAND = 0xF4,
	MESSAGE_PASSWORD_REQUIRED = 0xF5,
	MESSAGE_SETUP_REQUIRED = 0xF6,
	MESSAGE_CLIENT_SLOT_IN_USE = 0xF7,
	MESSAGE_CLIENT_SLOTS_EXHAUSTED = 0xF8,
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

int get_size_from_command(char* buf, unsigned len) {
	switch((uint8_t) buf[0]) {
		case MESSAGE_HELLO:
			return sizeof(HelloMessage);
		case MESSAGE_PASSWORD:
			if (len > 1) {
				// 2 byte for command and length + length of the password
				return 2 + buf[1];
			} else {
				return 0;
			}
		case MESSAGE_ABSINFO:
			return sizeof(ABSInfoMessage);
		case MESSAGE_DEVICE:
			if (len > 1) {
				// 3 bytes for command, length and type + sizeof(struct input_id) + length of the name
				return 3 + sizeof(struct input_id) + buf[1];
			} else {
				return 0;
			}
		case MESSAGE_SUCCESS:
			return sizeof(SuccessMessage);
		case MESSAGE_VERSION_MISMATCH:
			return sizeof(VersionMismatchMessage);
		case MESSAGE_HELLO_END:
		case MESSAGE_INVALID_PASSWORD:
		case MESSAGE_INVALID_CLIENT_SLOT:
		case MESSAGE_INVALID_COMMAND:
		case MESSAGE_PASSWORD_REQUIRED:
		case MESSAGE_SETUP_REQUIRED:
		case MESSAGE_CLIENT_SLOT_IN_USE:
		case MESSAGE_CLIENT_SLOTS_EXHAUSTED:
			return 1;
		case MESSAGE_DATA:
			return sizeof(DataMessage);
		default:
			return -1;
	}
}

#pragma once

#include <inttypes.h>
#include <linux/input.h>

#define PROTOCOL_VERSION 0x03
#define INPUT_BUFFER_SIZE 1024
#define DEFAULT_PASSWORD "foobar"
#define DEFAULT_HOST "::"
#define DEFAULT_PORT "9292"

#pragma pack(1)
enum DEV_TYPE {
	DEV_TYPE_UNKNOWN  = 0x00,
	DEV_TYPE_MOUSE    = 0x01,
	DEV_TYPE_KEYBOARD = 0x02,
	DEV_TYPE_GAMEPAD  = 0x04,
	DEV_TYPE_XBOX     = 0x08,
	DEV_TYPE_ABS      = 0x10,
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
	uint64_t type;
	uint16_t id_bustype;
	uint16_t id_vendor;
	uint16_t id_product;
	uint16_t id_version;
	char name[];
} DeviceMessage;

typedef struct {
	uint8_t msg_type;
	uint16_t type;
	uint16_t code;
	__s32 value;
} DataMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t version;
} VersionMismatchMessage;

typedef struct {
	uint8_t msg_type;
	uint8_t slot;
} SuccessMessage;

extern struct MessageInfo MESSAGE_TYPES_INFO[256];

char* get_message_name(uint8_t msg);

int get_size_from_command(uint8_t* buf, unsigned len);

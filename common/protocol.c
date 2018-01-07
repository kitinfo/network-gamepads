
#include <inttypes.h>
#include <linux/input.h>
#include "protocol.h"

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

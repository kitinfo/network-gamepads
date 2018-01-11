#include <inttypes.h>
#include "protocol.h"

struct MessageInfo MESSAGE_TYPES_INFO[256] = {
	[0 ... 255] = { .length = -1, .name = "Invalid Message"},
	[MESSAGE_HELLO] = { .length = sizeof(HelloMessage), .name = "Hello"},
	[MESSAGE_PASSWORD] = { .length = sizeof(PasswordMessage), .name = "Password"},
	[MESSAGE_ABSINFO] = {.length = sizeof(ABSInfoMessage), .name = "AbsInfo"},
	[MESSAGE_DEVICE] = { .length = sizeof(DeviceMessage), .name = "Device"},
	[MESSAGE_REQUEST_EVENT] = {.length = sizeof(RequestEventMessage), .name = "EventEnableRequest"},
	[MESSAGE_SETUP_END] = { .length = 1, .name = "SetupDone"},
	[MESSAGE_DATA] =  { .length = sizeof(DataMessage), .name = "Data"},
	[MESSAGE_SUCCESS] = { .length = sizeof(SuccessMessage), .name = "Success"},
	[MESSAGE_VERSION_MISMATCH] = { .length = sizeof(VersionMismatchMessage), .name = "VersionMismatch"},
	[MESSAGE_INVALID_PASSWORD] = { .length = 1, .name = "PasswordInvalid"},
	[MESSAGE_INVALID_CLIENT_SLOT] = { .length = 1, .name = "SlotInvalid"},
	[MESSAGE_INVALID] = { .length = 1, .name = "ProtocolError"},
	[MESSAGE_PASSWORD_REQUIRED] = { .length = 1, .name = "PasswordRequest"},
	[MESSAGE_SETUP_REQUIRED] = { .length = 1, .name = "SetupRequest"},
	[MESSAGE_CLIENT_SLOT_IN_USE] = { .length = 1, .name = "Occupied"},
	[MESSAGE_CLIENT_SLOTS_EXHAUSTED] = { .length = 1, .name = "Exhausted"},
	[MESSAGE_QUIT] = { .length = 1, .name = "Quit"},
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

#pragma once
#include <linux/input.h>
#include <linux/uinput.h>

#include "structures.h"

#define UINPUT_PATH "/dev/uinput"

const int UI_SET_BITS[] = {
	UI_SET_EVBIT,
	UI_SET_KEYBIT,
	UI_SET_RELBIT,
	UI_SET_ABSBIT,
	UI_SET_MSCBIT
};

typedef struct {
	int type;
	int value;
} Keybit;

typedef struct {
	int len;
	Keybit bits[];
} input_device_bits;

	/* DEFAULT */
const input_device_bits DEFAULT_KEYBITS = {
	.len = 1,
	.bits = {{ UI_SET_EVBIT, EV_SYN }}
};
	/* MICE */
const input_device_bits MICE_KEYBITS = {
	.len = 10,
	.bits = {
		{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_REL },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_KEYBIT, BTN_LEFT },
		{ UI_SET_KEYBIT, BTN_RIGHT },
		{ UI_SET_KEYBIT, BTN_MIDDLE },
		{ UI_SET_RELBIT, REL_X },
		{ UI_SET_RELBIT, REL_Y },
		{ UI_SET_RELBIT, REL_WHEEL },
	}
};
	/* GAMEPAD */
const input_device_bits GAMEPAD_KEYBITS = {
	.len = 23,
	.bits = {
		{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_ABS },
		{ UI_SET_EVBIT, EV_FF },
		{ UI_SET_KEYBIT, BTN_A },
		{ UI_SET_KEYBIT, BTN_B },
		{ UI_SET_KEYBIT, BTN_X },
		{ UI_SET_KEYBIT, BTN_Y },
		{ UI_SET_KEYBIT, BTN_TL },
		{ UI_SET_KEYBIT, BTN_TR },
		{ UI_SET_KEYBIT, BTN_SELECT },
		{ UI_SET_KEYBIT, BTN_START },
		{ UI_SET_KEYBIT, BTN_MODE },
		{ UI_SET_KEYBIT, BTN_THUMBL },
		{ UI_SET_KEYBIT, BTN_THUMBR },
		{ UI_SET_ABSBIT, ABS_X },
		{ UI_SET_ABSBIT, ABS_Y },
		{ UI_SET_ABSBIT, ABS_Z },
		{ UI_SET_ABSBIT, ABS_RX },
		{ UI_SET_ABSBIT, ABS_RY },
		{ UI_SET_ABSBIT, ABS_RZ },
		{ UI_SET_ABSBIT, ABS_HAT0X },
		{ UI_SET_ABSBIT, ABS_HAT0Y },
	}
};
	/* KEYBOARD */
const input_device_bits KEYBOARD_KEYBITS = {
	.len = 0,
	.bits = {}
};

const input_device_bits* DEVICE_TYPES[] = {
	&DEFAULT_KEYBITS,
	&MICE_KEYBITS,
	&GAMEPAD_KEYBITS,
	&KEYBOARD_KEYBITS
};
bool cleanup_device(LOGGER log, gamepad_client* client);
bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta);

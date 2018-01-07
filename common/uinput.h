#pragma once
#include <linux/input.h>
#include <linux/uinput.h>

#include "../libs/logger.h"
#include "structures.h"

#define UINPUT_PATH "/dev/uinput"

extern const int UI_SET_BITS[];

typedef struct {
	int type;
	int value;
} Keybit;

typedef struct {
	int len;
	Keybit bits[];
} input_device_bits;

	/* MICE */
extern const input_device_bits MICE_KEYBITS;

	/* ABS */
extern const input_device_bits ABS_KEYBITS;

	/* GAMEPAD */
extern const input_device_bits GAMEPAD_KEYBITS;

	/* XBOX */
extern const input_device_bits XBOX_KEYBITS;

	/* KEYBOARD */
extern const input_device_bits KEYBOARD_KEYBITS;

	/* DEFAULT */
extern const input_device_bits DEFAULT_KEYBITS;

const input_device_bits* DEVICE_TYPES[64];

bool cleanup_device(LOGGER log, gamepad_client* client);
void init_abs_info(struct device_meta* meta);
bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta);

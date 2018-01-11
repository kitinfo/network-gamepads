#pragma once
#include <linux/input.h>
#include <linux/uinput.h>

#include "../libs/logger.h"

#include "input-server.h"

#define UINPUT_PATH "/dev/uinput"

bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta);
bool cleanup_device(LOGGER log, gamepad_client* client);

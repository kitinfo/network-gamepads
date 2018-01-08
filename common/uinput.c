#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "../libs/logger.h"
#include "uinput.h"

const int UI_SET_BITS[] = {
	UI_SET_EVBIT,
	UI_SET_KEYBIT,
	UI_SET_RELBIT,
	UI_SET_ABSBIT,
	UI_SET_MSCBIT
};

	/* MICE */
const input_device_bits MICE_KEYBITS = {
	.len = 11,
	.bits = {
		{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_REL },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_MSCBIT, MSC_SCAN },
		{ UI_SET_KEYBIT, BTN_LEFT },
		{ UI_SET_KEYBIT, BTN_RIGHT },
		{ UI_SET_KEYBIT, BTN_MIDDLE },
		{ UI_SET_RELBIT, REL_X },
		{ UI_SET_RELBIT, REL_Y },
		{ UI_SET_RELBIT, REL_WHEEL },
	}
};

	/* ABS */
const input_device_bits ABS_KEYBITS = {
	.len = 9,
	.bits = {
		{ UI_SET_EVBIT, EV_ABS },
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
	/* GAMEPAD */
const input_device_bits GAMEPAD_KEYBITS = {
	.len = 41,
	.bits = {
		{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_ABS },
		//{ UI_SET_EVBIT, EV_FF },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_MSCBIT, MSC_SCAN },
		{ UI_SET_KEYBIT, BTN_A },
		{ UI_SET_KEYBIT, BTN_B },
		{ UI_SET_KEYBIT, BTN_C },
		{ UI_SET_KEYBIT, BTN_X },
		{ UI_SET_KEYBIT, BTN_Y },
		{ UI_SET_KEYBIT, BTN_TL },
		{ UI_SET_KEYBIT, BTN_TR },
		{ UI_SET_KEYBIT, BTN_SELECT },
		{ UI_SET_KEYBIT, BTN_START },
		{ UI_SET_KEYBIT, BTN_BACK },
		{ UI_SET_KEYBIT, BTN_MODE },
		{ UI_SET_KEYBIT, BTN_THUMBL },
		{ UI_SET_KEYBIT, BTN_THUMBR },
		{ UI_SET_KEYBIT, BTN_TRIGGER },
		{ UI_SET_KEYBIT, BTN_THUMB },
		{ UI_SET_KEYBIT, BTN_THUMB2 },
		{ UI_SET_KEYBIT, BTN_TOP },
		{ UI_SET_KEYBIT, BTN_TOP2 },
		{ UI_SET_KEYBIT, BTN_PINKIE },
		{ UI_SET_KEYBIT, BTN_BASE },
		{ UI_SET_KEYBIT, BTN_BASE2 },
		{ UI_SET_KEYBIT, BTN_BASE3 },
		{ UI_SET_KEYBIT, BTN_BASE4 },
		{ UI_SET_KEYBIT, BTN_BASE5 },
		{ UI_SET_KEYBIT, BTN_BASE6 },
		{ UI_SET_KEYBIT, KEY_HOMEPAGE },
		{ UI_SET_ABSBIT, ABS_X },
		{ UI_SET_ABSBIT, ABS_Y },
		{ UI_SET_ABSBIT, ABS_Z },
		{ UI_SET_ABSBIT, ABS_RX },
		{ UI_SET_ABSBIT, ABS_RY },
		{ UI_SET_ABSBIT, ABS_RZ },
		{ UI_SET_ABSBIT, ABS_HAT0X },
		{ UI_SET_ABSBIT, ABS_HAT0Y },
		{ UI_SET_ABSBIT, ABS_GAS },
		{ UI_SET_ABSBIT, ABS_BRAKE },
	}
};
	/* XBOX */
const input_device_bits XBOX_KEYBITS = {
	.len = 28,
	.bits = {
		{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_ABS },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_MSCBIT, MSC_SCAN },
		{ UI_SET_KEYBIT, BTN_A },
		{ UI_SET_KEYBIT, BTN_B },
		{ UI_SET_KEYBIT, BTN_X },
		{ UI_SET_KEYBIT, BTN_Y },
		{ UI_SET_KEYBIT, BTN_TL },
		{ UI_SET_KEYBIT, BTN_TR },
		{ UI_SET_KEYBIT, BTN_SELECT },
		{ UI_SET_KEYBIT, BTN_START },
		{ UI_SET_KEYBIT, BTN_BACK },
		{ UI_SET_KEYBIT, BTN_MODE },
		{ UI_SET_KEYBIT, BTN_THUMBL },
		{ UI_SET_KEYBIT, BTN_THUMBR },
		{ UI_SET_KEYBIT, KEY_HOMEPAGE },
		{ UI_SET_ABSBIT, ABS_X },
		{ UI_SET_ABSBIT, ABS_Y },
		{ UI_SET_ABSBIT, ABS_Z },
		{ UI_SET_ABSBIT, ABS_RX },
		{ UI_SET_ABSBIT, ABS_RY },
		{ UI_SET_ABSBIT, ABS_RZ },
		{ UI_SET_ABSBIT, ABS_HAT0X },
		{ UI_SET_ABSBIT, ABS_HAT0Y },
		{ UI_SET_ABSBIT, ABS_GAS },
		{ UI_SET_ABSBIT, ABS_BRAKE },
	}
};
	/* KEYBOARD */
const input_device_bits KEYBOARD_KEYBITS = {
	.len = 139,
	.bits = {{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_KEY },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_MSCBIT, MSC_SCAN },
		{ UI_SET_KEYBIT, KEY_ESC },
		{ UI_SET_KEYBIT, KEY_1 },
		{ UI_SET_KEYBIT, KEY_2 },
		{ UI_SET_KEYBIT, KEY_3 },
		{ UI_SET_KEYBIT, KEY_4 },
		{ UI_SET_KEYBIT, KEY_5 },
		{ UI_SET_KEYBIT, KEY_6 },
		{ UI_SET_KEYBIT, KEY_7 },
		{ UI_SET_KEYBIT, KEY_8 },
		{ UI_SET_KEYBIT, KEY_9 },
		{ UI_SET_KEYBIT, KEY_0 },
		{ UI_SET_KEYBIT, KEY_MINUS },
		{ UI_SET_KEYBIT, KEY_EQUAL },
		{ UI_SET_KEYBIT, KEY_BACKSPACE },
		{ UI_SET_KEYBIT, KEY_TAB },
		{ UI_SET_KEYBIT, KEY_Q },
		{ UI_SET_KEYBIT, KEY_W },
		{ UI_SET_KEYBIT, KEY_E },
		{ UI_SET_KEYBIT, KEY_R },
		{ UI_SET_KEYBIT, KEY_T },
		{ UI_SET_KEYBIT, KEY_Y },
		{ UI_SET_KEYBIT, KEY_U },
		{ UI_SET_KEYBIT, KEY_I },
		{ UI_SET_KEYBIT, KEY_O },
		{ UI_SET_KEYBIT, KEY_P },
		{ UI_SET_KEYBIT, KEY_LEFTBRACE },
		{ UI_SET_KEYBIT, KEY_RIGHTBRACE },
		{ UI_SET_KEYBIT, KEY_ENTER },
		{ UI_SET_KEYBIT, KEY_LEFTCTRL },
		{ UI_SET_KEYBIT, KEY_A },
		{ UI_SET_KEYBIT, KEY_S },
		{ UI_SET_KEYBIT, KEY_D },
		{ UI_SET_KEYBIT, KEY_F },
		{ UI_SET_KEYBIT, KEY_G },
		{ UI_SET_KEYBIT, KEY_H },
		{ UI_SET_KEYBIT, KEY_J },
		{ UI_SET_KEYBIT, KEY_K },
		{ UI_SET_KEYBIT, KEY_L },
		{ UI_SET_KEYBIT, KEY_SEMICOLON },
		{ UI_SET_KEYBIT, KEY_APOSTROPHE },
		{ UI_SET_KEYBIT, KEY_GRAVE },
		{ UI_SET_KEYBIT, KEY_LEFTSHIFT },
		{ UI_SET_KEYBIT, KEY_BACKSLASH },
		{ UI_SET_KEYBIT, KEY_Z },
		{ UI_SET_KEYBIT, KEY_X },
		{ UI_SET_KEYBIT, KEY_C },
		{ UI_SET_KEYBIT, KEY_V },
		{ UI_SET_KEYBIT, KEY_B },
		{ UI_SET_KEYBIT, KEY_N },
		{ UI_SET_KEYBIT, KEY_M },
		{ UI_SET_KEYBIT, KEY_COMMA },
		{ UI_SET_KEYBIT, KEY_DOT },
		{ UI_SET_KEYBIT, KEY_SLASH },
		{ UI_SET_KEYBIT, KEY_RIGHTSHIFT },
		{ UI_SET_KEYBIT, KEY_KPASTERISK },
		{ UI_SET_KEYBIT, KEY_LEFTALT },
		{ UI_SET_KEYBIT, KEY_SPACE },
		{ UI_SET_KEYBIT, KEY_CAPSLOCK },
		{ UI_SET_KEYBIT, KEY_F1 },
		{ UI_SET_KEYBIT, KEY_F2 },
		{ UI_SET_KEYBIT, KEY_F3 },
		{ UI_SET_KEYBIT, KEY_F4 },
		{ UI_SET_KEYBIT, KEY_F5 },
		{ UI_SET_KEYBIT, KEY_F6 },
		{ UI_SET_KEYBIT, KEY_F7 },
		{ UI_SET_KEYBIT, KEY_F8 },
		{ UI_SET_KEYBIT, KEY_F9 },
		{ UI_SET_KEYBIT, KEY_F10 },
		{ UI_SET_KEYBIT, KEY_NUMLOCK },
		{ UI_SET_KEYBIT, KEY_SCROLLLOCK },
		{ UI_SET_KEYBIT, KEY_KP7 },
		{ UI_SET_KEYBIT, KEY_KP8 },
		{ UI_SET_KEYBIT, KEY_KP9 },
		{ UI_SET_KEYBIT, KEY_KPMINUS },
		{ UI_SET_KEYBIT, KEY_KP4 },
		{ UI_SET_KEYBIT, KEY_KP5 },
		{ UI_SET_KEYBIT, KEY_KP6 },
		{ UI_SET_KEYBIT, KEY_KPPLUS },
		{ UI_SET_KEYBIT, KEY_KP1 },
		{ UI_SET_KEYBIT, KEY_KP2 },
		{ UI_SET_KEYBIT, KEY_KP3 },
		{ UI_SET_KEYBIT, KEY_KP0 },
		{ UI_SET_KEYBIT, KEY_KPDOT },
		{ UI_SET_KEYBIT, KEY_ZENKAKUHANKAKU },
		{ UI_SET_KEYBIT, KEY_102ND },
		{ UI_SET_KEYBIT, KEY_F11 },
		{ UI_SET_KEYBIT, KEY_F12 },
		{ UI_SET_KEYBIT, KEY_RO },
		{ UI_SET_KEYBIT, KEY_KATAKANA },
		{ UI_SET_KEYBIT, KEY_HIRAGANA },
		{ UI_SET_KEYBIT, KEY_HENKAN },
		{ UI_SET_KEYBIT, KEY_KATAKANAHIRAGANA },
		{ UI_SET_KEYBIT, KEY_MUHENKAN },
		{ UI_SET_KEYBIT, KEY_KPJPCOMMA },
		{ UI_SET_KEYBIT, KEY_KPENTER },
		{ UI_SET_KEYBIT, KEY_RIGHTCTRL },
		{ UI_SET_KEYBIT, KEY_KPSLASH },
		{ UI_SET_KEYBIT, KEY_SYSRQ },
		{ UI_SET_KEYBIT, KEY_RIGHTALT },
		{ UI_SET_KEYBIT, KEY_LINEFEED },
		{ UI_SET_KEYBIT, KEY_HOME },
		{ UI_SET_KEYBIT, KEY_UP },
		{ UI_SET_KEYBIT, KEY_PAGEUP },
		{ UI_SET_KEYBIT, KEY_LEFT },
		{ UI_SET_KEYBIT, KEY_RIGHT },
		{ UI_SET_KEYBIT, KEY_END },
		{ UI_SET_KEYBIT, KEY_DOWN },
		{ UI_SET_KEYBIT, KEY_PAGEDOWN },
		{ UI_SET_KEYBIT, KEY_INSERT },
		{ UI_SET_KEYBIT, KEY_DELETE },
		{ UI_SET_KEYBIT, KEY_KPEQUAL },
		{ UI_SET_KEYBIT, KEY_KPPLUSMINUS },
		{ UI_SET_KEYBIT, KEY_PAUSE },
		{ UI_SET_KEYBIT, KEY_SCALE },
		{ UI_SET_KEYBIT, KEY_KPCOMMA },
		{ UI_SET_KEYBIT, KEY_HANGEUL },
		{ UI_SET_KEYBIT, KEY_HANJA },
		{ UI_SET_KEYBIT, KEY_YEN },
		{ UI_SET_KEYBIT, KEY_LEFTMETA },
		{ UI_SET_KEYBIT, KEY_RIGHTMETA },
		{ UI_SET_KEYBIT, KEY_COMPOSE },
		{ UI_SET_KEYBIT, KEY_SCROLLUP },
		{ UI_SET_KEYBIT, KEY_SCROLLDOWN },
		{ UI_SET_KEYBIT, KEY_F13 },
		{ UI_SET_KEYBIT, KEY_F14 },
		{ UI_SET_KEYBIT, KEY_F15 },
		{ UI_SET_KEYBIT, KEY_F16 },
		{ UI_SET_KEYBIT, KEY_F17 },
		{ UI_SET_KEYBIT, KEY_F18 },
		{ UI_SET_KEYBIT, KEY_F19 },
		{ UI_SET_KEYBIT, KEY_F20 },
		{ UI_SET_KEYBIT, KEY_F21 },
		{ UI_SET_KEYBIT, KEY_F22 },
		{ UI_SET_KEYBIT, KEY_F23 },
		{ UI_SET_KEYBIT, KEY_F24 },
	}
};

	/* DEFAULT */
const input_device_bits DEFAULT_KEYBITS = {
	.len = 3,
	.bits = {{ UI_SET_EVBIT, EV_SYN },
		{ UI_SET_EVBIT, EV_MSC },
		{ UI_SET_MSCBIT, MSC_SCAN },
	}
};

const input_device_bits* DEVICE_TYPES[64] = {
	[0 ... 63] = NULL,
	[0] = &MICE_KEYBITS,
	&KEYBOARD_KEYBITS,
	&GAMEPAD_KEYBITS,
	&XBOX_KEYBITS,
	&ABS_KEYBITS,
};

int open_uinput() {
	return open(UINPUT_PATH, O_WRONLY | O_NONBLOCK);
}

bool enable_bits(LOGGER log, int fd, const input_device_bits* what) {
	int i;
	int ret;

	if (what == NULL) {
		return true;
	}

	for (i = 0; i < what->len; i++) {
		logprintf(log, LOG_DEBUG, "enable bit: %d\n", what->bits[i].value);
		ret = ioctl(fd, what->bits[i].type, what->bits[i].value);
		if (ret < 0) {
			logprintf(log, LOG_ERROR, "Cannot enable type %d-%d: %s\n", what->bits[i].type, what->bits[i].value, strerror(errno));
			return false;
		}
	}
	return true;
}

bool enable_device_keys(LOGGER log, int fd, struct device_meta* meta) {
	unsigned u;
	uint64_t mask = 1;
	for (u = 0; u < 64; u++) {
		if (meta->devtype & mask) {
			logprintf(log, LOG_INFO, "enable device keys: 0x%x - 0x%x\n", mask, u);
			if (!enable_bits(log, fd, DEVICE_TYPES[u])) {
				return false;
			}
		}

		mask <<= 1;
	}
	return true;
}

bool create_device_old_format(LOGGER log, gamepad_client* client, struct device_meta* meta) {
	struct uinput_user_dev dev = {};
	int uinput_fd = open_uinput();
	size_t u;
	if (uinput_fd < 0) {
		logprintf(log, LOG_ERROR, "Failed to access uinput: %s\n", strerror(errno));
		return false;
	}
	if (!enable_device_keys(log, uinput_fd, meta)) {
		logprintf(log, LOG_ERROR, "Failed to enable uinput keys\n");
		close(uinput_fd);
		return false;
	}

	memset(&dev, 0, sizeof(dev));
	memcpy(&dev.id, &meta->id, sizeof(struct input_id));
	strncpy(dev.name, meta->name, UINPUT_MAX_NAME_SIZE - 1);

	for(u = 0; u < ABS_CNT; u++){
		dev.absmin[u] = meta->absinfo[u].minimum;
		dev.absmax[u] = meta->absinfo[u].maximum;
		dev.absfuzz[u] = meta->absinfo[u].fuzz;
		dev.absflat[u] = meta->absinfo[u].flat;
	}

	int ret = write(uinput_fd, &dev, sizeof(dev));

	if (ret < 0) {
		logprintf(log, LOG_ERROR, "Failed to write to uinput FD: %s\n", strerror(errno));
		close(uinput_fd);
		return false;
	}

	ret = ioctl(uinput_fd, UI_DEV_CREATE);

	if (ret < 0) {
		close(uinput_fd);
		logprintf(log, LOG_ERROR, "Failed to create device: %s\n", strerror(errno));
		return false;
	}
	client->ev_fd = uinput_fd;
	return true;

}

bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta) {
	int uinput_fd = open_uinput();
	size_t u;

	if (uinput_fd < 0) {
		logprintf(log, LOG_ERROR, "Failed to access uinput: %s\n", strerror(errno));
		return false;
	}

	if (!enable_device_keys(log, uinput_fd, meta)) {
		logprintf(log, LOG_ERROR, "Failed to enable uinput keys\n");
		close(uinput_fd);
		return false;
	}

	struct uinput_setup dev = {};
	memset(&dev, 0, sizeof(dev));
	memcpy(&dev.id, &meta->id, sizeof(struct input_id));
	strncpy(dev.name, meta->name, UINPUT_MAX_NAME_SIZE - 1);

	if (ioctl(uinput_fd, UI_DEV_SETUP, &dev) < 0) {
		logprintf(log, LOG_WARNING, "Cannot setup device, try old ioctl.\n");
		close(uinput_fd);
		return create_device_old_format(log, client, meta);
	}
	for (u = 0; u < ABS_CNT; u++) {
		if (ioctl(uinput_fd, UI_ABS_SETUP, meta->absinfo + u) < 0) {
			logprintf(log, LOG_WARNING, "Cannot setup absinfo, try old ioctl.\n");
			close(uinput_fd);
			return create_device_old_format(log, client, meta);
		}
	}
	int ret = ioctl(uinput_fd, UI_DEV_CREATE);

	if (ret < 0) {
		close(uinput_fd);
		logprintf(log, LOG_WARNING, "Cannot create device, try old ioctl.\n");
		return create_device_old_format(log, client, meta);
	}
	client->ev_fd = uinput_fd;
	return true;
}

bool cleanup_device(LOGGER log, gamepad_client* client) {
	if (client->ev_fd < 0) {
		return true;
	}
	int ret = ioctl(client->ev_fd, UI_DEV_DESTROY);

	if (ret < 0) {
		logprintf(log, LOG_ERROR, "Cannot destroy device: %s\n", strerror(errno));
		return false;
	}
	close(client->ev_fd);
	client->ev_fd =  -1;

	if (client->meta.name) {
		free(client->meta.name);
		client->meta.name = NULL;
	}

	return true;
}

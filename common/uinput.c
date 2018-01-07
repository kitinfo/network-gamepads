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
void init_abs_info(struct device_meta* meta) {
		// ABS_X
		meta->absmin[ABS_X] = -32768;
		meta->absmax[ABS_X] = 32767;
		meta->absfuzz[ABS_X] = 16;
		meta->absflat[ABS_X] = 128;
		// ABS_Y
		meta->absmin[ABS_Y] = -32768;
		meta->absmax[ABS_Y] = 32767;
		meta->absfuzz[ABS_Y] = 16;
		meta->absflat[ABS_Y] = 128;
		// ABS_Z
		meta->absmin[ABS_Z] = 0;
		meta->absmax[ABS_Z] = 255;
		// ABS_RX
		meta->absmin[ABS_RX] = -32768;
		meta->absmax[ABS_RX] = 32767;
		meta->absfuzz[ABS_RX] = 16;
		meta->absflat[ABS_RX] = 128;
		// ABS_RY
		meta->absmin[ABS_RY] = -32768;
		meta->absmax[ABS_RY] = 32767;
		meta->absfuzz[ABS_RY] = 16;
		meta->absflat[ABS_RY] = 128;
		// ABS_RZ
		meta->absmin[ABS_RZ] = 0;
		meta->absmax[ABS_RZ] = 255;
		// ABS_HAT0X
		meta->absmin[ABS_HAT0X] = -1;
		meta->absmax[ABS_HAT0X] = 1;
		// ABS_HAT0Y
		meta->absmin[ABS_HAT0Y] = -1;
		meta->absmax[ABS_HAT0Y] = 1;
}

bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta) {
	int uinput_fd = open_uinput();
	if (uinput_fd < 0) {
		logprintf(log, LOG_ERROR, "Failed to access uinput: %s\n", strerror(errno));
		return false;
	}
	if (!enable_device_keys(log, uinput_fd, meta)) {
		logprintf(log, LOG_ERROR, "Failed to enable uinput keys\n");
		close(uinput_fd);
		return false;
	}
	struct uinput_user_dev dev = {};
	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, meta->name, UINPUT_MAX_NAME_SIZE - 1);
	memcpy(&dev.id, &meta->id, sizeof(struct input_id));
	memcpy(&dev.absmax, &meta->absmax, ABS_CNT * sizeof(__s32));
	memcpy(&dev.absmin, &meta->absmin, ABS_CNT * sizeof(__s32));
	memcpy(&dev.absfuzz, &meta->absfuzz, ABS_CNT * sizeof(__s32));
	memcpy(&dev.absflat, &meta->absflat, ABS_CNT * sizeof(__s32));
	int ret = write(uinput_fd, &dev, sizeof(dev));

	if (ret < 0) {
		logprintf(log, LOG_ERROR, "Cannot write to uinput device: %s\n", strerror(errno));
		close(uinput_fd);
		return false;
	}

	ret = ioctl(uinput_fd, UI_DEV_CREATE);

	if (ret < 0) {
		close(uinput_fd);
		logprintf(log, LOG_ERROR, "Cannot create device: %s\n", strerror(errno));
		return false;
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
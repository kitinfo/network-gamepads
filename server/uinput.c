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

struct {
	unsigned long event;
	unsigned long type;
} enable_map[] = {
	//EV_SYN handled externally
	{EV_KEY, UI_SET_KEYBIT},
	{EV_REL, UI_SET_RELBIT},
	{EV_ABS, UI_SET_ABSBIT},
	{EV_MSC, UI_SET_MSCBIT},
	{0, 0}
};

static bool enable_events(LOGGER log, int fd, struct device_meta* meta){
	size_t u, p;
	unsigned long last_type = EV_MAX;

	for(u = 0; u < meta->enabled_events_length; u++){
		//find matching type/code, also serves as input validation
		for(p = 0; enable_map[p].event && enable_map[p].event != meta->enabled_events[u].type; p++){
		}

		//enable syn
		if(!enable_map[p].event){
			if(ioctl(fd, UI_SET_EVBIT, EV_SYN)){
				logprintf(log, LOG_ERROR, "Failed to enable SYN events\n");
				return false;
			}
			continue;
		}

		if(last_type != meta->enabled_events[u].type){
			//enable event type
			if(ioctl(fd, UI_SET_EVBIT, enable_map[p].event)){
				logprintf(log, LOG_ERROR, "Failed to enable event type %02X\n", meta->enabled_events[u].type);
				return false;
			}

			last_type = meta->enabled_events[u].type;
		}

		//enable key
		if(ioctl(fd, enable_map[p].type, meta->enabled_events[u].code)){
			logprintf(log, LOG_ERROR, "Failed to enable event type %02X code %X\n", meta->enabled_events[u].type, meta->enabled_events[u].code);
			return false;
		}
	}

	return true;
}

bool create_device(LOGGER log, gamepad_client* client, struct device_meta* meta) {
	size_t u;
	int uinput_version, uinput_fd = open(UINPUT_PATH, O_WRONLY | O_NONBLOCK);
	struct uinput_setup ui_setup = {
		.id = meta->id
	};
	struct uinput_user_dev ui_dev = {
		.id = meta->id
	};

	if(uinput_fd < 0) {
		logprintf(log, LOG_ERROR, "Failed to access uinput: %s\n", strerror(errno));
		return false;
	}

	if(ioctl(uinput_fd, UI_GET_VERSION, &uinput_version)){
		logprintf(log, LOG_ERROR, "Failed to query uinput version\n");
		close(uinput_fd);
		return false;
	}

	logprintf(log, LOG_INFO, "Detected uinput version %d\n", uinput_version);

	if(!enable_events(log, uinput_fd, meta)){
		logprintf(log, LOG_ERROR, "Failed to enable requested input events\n");
		close(uinput_fd);
		return false;
	}

	if(uinput_version >= 5){
		//new ioctl setup method
		logprintf(log, LOG_WARNING, "Using new-style device setup\n");
		strncpy(ui_setup.name, meta->name, UINPUT_MAX_NAME_SIZE - 1);
		if(ioctl(uinput_fd, UI_DEV_SETUP, &ui_setup)) {
			logprintf(log, LOG_WARNING, "Failed to create uinput device: %s\n", strerror(errno));
		}
		else{
			//set up absolute axes
			for(u = 0; u < ABS_CNT; u++){
				if(ioctl(uinput_fd, UI_ABS_SETUP, meta->absinfo + u)){
					logprintf(log, LOG_WARNING, "Failed to set up absolute axis: %s\n", strerror(errno));
					close(uinput_fd);
					return false;
				}
			}
		}
	}
	else{
		//deprecated setup
		logprintf(log, LOG_WARNING, "Using deprecated device setup\n");
		strncpy(ui_dev.name, meta->name, UINPUT_MAX_NAME_SIZE - 1);
	
		for(u = 0; u < ABS_CNT; u++){
			ui_dev.absmin[u] = meta->absinfo[u].minimum;
			ui_dev.absmax[u] = meta->absinfo[u].maximum;
			ui_dev.absfuzz[u] = meta->absinfo[u].fuzz;
			ui_dev.absflat[u] = meta->absinfo[u].flat;
		}

		if(write(uinput_fd, &ui_dev, sizeof(ui_dev)) < 0){
			logprintf(log, LOG_ERROR, "Failed to set up device: %s\n", strerror(errno));
			close(uinput_fd);
			return false;
		}
	}

	if(ioctl(uinput_fd, UI_DEV_CREATE)){
		logprintf(log, LOG_WARNING, "Failed to finalize device creation: %s\n", strerror(errno));
		close(uinput_fd);
		return false;
	}
	client->ev_fd = uinput_fd;
	return true;
}

bool cleanup_device(LOGGER log, gamepad_client* client) {
	if (client->ev_fd < 0) {
		return true;
	}

	if(ioctl(client->ev_fd, UI_DEV_DESTROY)){
		logprintf(log, LOG_ERROR, "Failed to destroy uinput device: %s\n", strerror(errno));
		return false;
	}

	close(client->ev_fd);
	client->ev_fd = -1;

	free(client->meta.name);
	client->meta.name = NULL;
	return true;
}

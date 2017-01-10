#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

enum DEV_TYPE {
	DEV_TYPE_UNKOWN = 0,
	DEV_TYPE_MICE = 1,
	DEV_TYPE_GAMEPAD = 2,
	DEV_TYPE_KEYBOARD  = 3
};

struct device_meta {
	int vendor_id;
	int product_id;
	int bustype;
	int devtype;
	int version;
	char* name;
};

struct libevdev* evdev_node(struct device_meta* meta){
	int i;
	struct libevdev* dev = libevdev_new();

	//libevdev_set_uniq(dev, "23456");
	libevdev_set_id_version(dev, meta->version);
	libevdev_set_id_vendor(dev, meta->vendor_id);
	libevdev_set_id_bustype(dev, meta->bustype);
	libevdev_set_id_product(dev, meta->product_id);
	//libevdev_set_name(dev, "Gamepad-Server Virtual Device");
	libevdev_set_name(dev, meta->name);
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_type(dev, EV_ABS);
	libevdev_enable_event_type(dev, EV_REL);

	// syn types
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_code(dev, EV_SYN, SYN_MT_REPORT, NULL);
	libevdev_enable_event_code(dev, EV_SYN, SYN_DROPPED, NULL);
	libevdev_enable_event_code(dev, EV_SYN, SYN_REPORT, NULL);

	// buttons
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_code(dev, EV_KEY, BTN_A, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_B, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_X, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_Y, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TL, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TR, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TRIGGER, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TOP, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_TOP2, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE2, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE3, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE4, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE5, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BASE6, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_SELECT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_MODE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_START, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_PINKIE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMB, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMB2, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMBL, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_THUMBR, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_UP, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_DOWN, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_LEFT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_RIGHT, NULL);
	for (i = 0; i < 128; i++) {
		libevdev_enable_event_code(dev, EV_KEY, i, NULL);
	}

	// mouse clicks
	libevdev_enable_event_code(dev, EV_KEY, BTN_MOUSE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_WHEEL, NULL);

	// rel
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Z, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RX, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RY, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_RZ, NULL);

	// hacky absinfo from xbox controller
	struct input_absinfo absinfo = {
		.value = -2866,
		.minimum = -32768,
		.maximum = 32767,
		.fuzz = 16,
		.flat = 128
	};

	// abs
	libevdev_enable_event_type(dev, EV_ABS);
	libevdev_enable_event_code(dev, EV_ABS, ABS_X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_Z, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RX, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RY, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_RZ, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT0X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT0Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT1X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT1Y, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT2X, &absinfo);
	libevdev_enable_event_code(dev, EV_ABS, ABS_HAT2Y, &absinfo);

	return dev;
}

struct libevdev_uinput* evdev_input(struct libevdev* device){
	struct libevdev_uinput* input = NULL;
	if(libevdev_uinput_create_from_device(device, LIBEVDEV_UINPUT_OPEN_MANAGED, &input) != 0) {
		fprintf(stderr, "Failed to create input device\n");
		return NULL;
	}
	fprintf(stderr, "Created input device: %s\n", libevdev_uinput_get_devnode(input));
	return input;
}



typedef struct /*_GAMEPAD_CLIENT*/ {
	int fd;
	struct libevdev* ev_device;
	struct libevdev_uinput* ev_input;
	int vendor_id;
	int product_id;
	char* dev_name;
	bool passthru;
	size_t scan_offset;
	char token[TOKEN_SIZE + 1];
	uint8_t input_buffer[INPUT_BUFFER_SIZE];
} gamepad_client;

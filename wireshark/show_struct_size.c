#include <stdio.h>
#include <stdint.h>
#include "../protocol.h"

int main() {

	printf("HELLO: %zd\n", sizeof(HelloMessage));
	printf("PASSWORD: %zd\n", sizeof(PasswordMessage));
	printf("ABSInfo: %zd\n", sizeof(ABSInfoMessage));
	printf("DEVICE: %zd\n", sizeof(DeviceMessage));
	printf("DATA: %zd\n", sizeof(DataMessage));
	printf("VERSION_MISMATCH: %zd\n", sizeof(VersionMismatchMessage));
	printf("SUCCESS: %zd\n", sizeof(SuccessMessage));

	return 0;
}

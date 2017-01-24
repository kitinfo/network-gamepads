Network gamepads protocol documentation. Version: 0x03

# Security

* Do not use this protocol to send your keyboard over the internet.
* The connection is not secure.
* Use it only in environments you control.
* It has no encryption so do not send important passwords with this protocol.
* Don't use a server password that you use on other services.

If you need to send keyboard to a remote x session use something like this:
```bash
	ssh -X <server> x2x -to :0 -nomouse
```
# Introduction

This is the first draft for the network gamepad binary protocol.

The binary protocol is splitted into messages. Every message begins
with a one message type byte (uint8_t). After this the data part of the
message is followed.

# Overview of message types
| Message name           | Message id |
|------------------------|------------|
| HELLO                  | 0x01       |
| PASSWORD               | 0x02       |
| ABSINFO                | 0x03       |
| DEVICE                 | 0x04       |
| SETUP_END              | 0x05       |
| DATA                   | 0x10       |
| SUCCESS                | 0xF0       |
| VERSION_MISMATCH       | 0xF1       |
| INVALID_PASSWORD       | 0xF2       |
| INVALID_CLIENT_SLOT    | 0xF3       |
| INVALID_MESSAGE        | 0xF4       |
| PASSWORD_REQUIRED      | 0xF5       |
| SETUP_REQUIRED         | 0xF6       |
| CLIENT_SLOT_IN_USE     | 0xF7       |
| CLIENT_SLOTS_EXHAUSTED | 0xF8       |
| QUIT                   | 0xF9       |
| DEVICE_NOT_ALLOWED     | 0xFA       |

# Messages

## Hello

```c
struct HelloMessage {
	uint8_t msg_type; /* must be 0x01 */
	uint8_t version; /* must be PROTOCOL_VERSION (0x03) */
	uint8_t slot; /* the slot. */
}
```

The HELLO message must be send only at the beginning of a connection.
Its data part contains only the version of the used protocol The version is
one byte long (uint8_t).
The current version is 0x02.

After the version byte follows a client slot byte. This byte is for selecting
a slot on the server. If the byte is 0x00 the server selects a slot for
client.

If the protocol version of the server mismatches the sended version the server
replies with the VERSION_MISMATCH reply.

If the HELLO message is not sended at the beginning of the connection the
server replies with INVALID_MESSAGE.

If the client slot is out of range the server send the INVALID_CLIENT_SLOT
message. The server can set the slot range but can not accept more than 255
clients.
If the client slot is already in use the server response with
CLIENT_SLOT_IN_USE.

If the client request not specific slot but all slots are exhausted then the
server send the CLIENT_SLOTS_EXHAUSTED message.

If everything is fine the server respond with SETUP_REQUIRED if a device setup
is required or when a
password is required PASSWORD_REQUIRED or SUCCESS if everything is set up.

Example:

0x01 0x01 0x00
(Message type: HELLO; Version: 0x01; Client slot: next free)

## PASSWORD

```c
struct PasswordMessage {
	uint8_t msg_type; /* must be 0x02 */
	uint8_t length; /* length of password */
	uint8_t password; /* The password. Must be of the given length. */
}
```

If the server requires a password it sends the PASSWORD_REQUIRED answer.
After that the client must respond with the PASSWORD message. Any other
message results in the INVALID_MESSAGE respond.

The message type byte is followed by a password length byte and then followed by
the password. Because of one byte (uint8_t) the maximal length of a password is
limited to 255 byte. The password encoding should be ascii but is not limited
to it.

Example:
0x02 0x04 0x48 0x45 0x4C 0x4F
(Message type byte, length of the password is 4 byte, followed by HELO)

If the password is incorrect the server responds with INVALID_PASSWORD and
closes the connection.

If the password is correct the server responds with SETUP_REQUIRED or with
SUCCESS if the client slot has already been set up. This is only the case if
the client sends a valid client id in the HELLO message.

## DEVICE

```c
struct DeviceMessage {
	uint8_t msg_type; /* must be 0x04 */
	uint8_t length; /* length of the name */
	uint64_t type; /* see device types */
	struct input_id ids; /* see input.h */
	char name[]; /* must be of the given length */
}

```

After getting a SETUP_REQUIRED event from the server. The client should
respond with the DEVICE message. This message contains infos to create the
uinput device on the server.
It contains the following infos:
* length (uint8_t): length of the name. The name should not be longer than
                    UINPUT_MAX_NAME_SIZE (see linux/uinput.h)
* type (uint8_t): device type (see "Device types" for valid values)
* ids (struct input_id): This is a struct from linux/input.h with the vendor id,
                         product _id, bustype and version of the device.
* name: a char array with the name of the device.

The server will not respond until the SETUP_END message will be send.

### Device types

Device types enable different input events on the server.
Device types can be combined (mouse | keyboard for keyboards with trackball or so).

| name     | value   | description     |
|----------|---------|-----------------|
| unknown  | 0x0000  | Unknown device  |
| mouse    | 0x0001  | Mouse device    |
| keyboard | 0x0002  | Keyboard device |
| gamepad  | 0x0004  | Gamepad device  |
| xbox     | 0x0008  | xbox gamepad    |

## ABSINFO

```c
struct ABSInfoMessage {
	uint8_t msg_type; /* must be 0x03 */
	uint8_t axis; /* must be smaller than MSG_MAX (see linux/input-event-codes.h)
	struct input_absinfo info; /* see linux/input.h */
}
```

This message contains informations about an axis of an absolute device.
The client should send this message for every axis of the device.

The second byte in the message is the axis information. See linux/input.h for
all available axis (ABS_*). The rest of the message is the absinfo for the
axis. For the right implementation see linux/input.h for the corresponding
struct.

The server will not respond until the SETUP_END message will be send.

## SETUP_END

```c
struct SetupEndMessage {
	uint8_t msg_type; /* must be 0x05 */
}
```

The message contains only the message type byte. It must be send to leave the
setup mode.

After the message send the server can answer with SUCCESS if everything is
fine.

## DATA

```c
struct DataMessage {
	uint8_t msg_type; /* must be 0x10 */
	struct input_event event; /* see linux/input.h */
}
```

This send an input event to the server.

After the message type byte comes the input event. See linux/input.h for the
informations on the input_event struct.
IMPORTANT: The struct timeval must be of the same size as the struct timeval
on the server (currently 16 bytes at most systems).

The server responds currently only on error with INVALID_MESSAGE when the data
message isn't sended in success state of the server.

## SUCCESS

```c
struct SuccessMessage {
	uint8_t msg_type; /* must be 0xF0 */
	uint8_t slot;
}
```

This message signals that the server is ready to process data events.
The message contains one byte with the slot. This can be used to reconnect to
the server without the setup overhead.

After the client receives this message, DATA messages can be send.


## VERSION_MISMATCH

```c
struct VersionMismatchMessage {
	uint8_t msg_type; /* must be 0xF1 */
	uint8_t version; /* version of the server */
}
```

This message can occur as responds to the HELLO message.
It signals that the protocol version of the client and the version of the
server are not the same.

The message contains the server version so that the client can show it.

After this message the server closes the connection.

## INVALID_PASSWORD

```c
struct InvalidPasswordMessage {
	uint8_t msg_type; /* must be 0xF2 */
}
```

This message can occur as respond to the PASSWORD message.
It signals that the given password was incorrect.
After this message the server closes the connection.

## INVALID_CLIENT_SLOT

```c
struct InvalidClientSlotMessage {
	uint8_t msg_type; /* must be 0xF3 */
}
```

This messages can occur after the HELLO message.
It signals that the sended client slot is not valid (greater than the maximum
client slots of the server).

After this message the server closes the connection.

## INVALID_MESSAGE

```c
struct InvalidMessage {
	uint8_t msg_type; /* must be 0xF4 */
}
```

This message occurs as respond to an message with an unkown message type or
when a message is send in the wrong server connection state.

After this message the server closes the connection.

## PASSWORD_REQUIRED

```c
struct PasswordRequiredMessage {
	uint8_t msg_type; /* must be 0xF5 */
}
```

This message is the responds to the hello message, when a server password is
set. After this message the client must respond with the PASSWORD message.

## SETUP_REQUIRED

```c
struct SetupRequiredMessage {
	uint8_t msg_type; /* must be 0xF6 */
}
```

This message can be a respond the HELLO message or to the PASSWORD message.
It signals that the server must set up the uinput device.
The client must send device set up informations (DEVICE or/and ABSINFO
message(s)). To leave the setup connection state the client must send the
SETUP_END message.

The client can send this message in success connection state, too. This
signals that the client has different device informations. The server answers
with this message and enters the setup connection state.

## CLIENT_SLOT_IN_USE

```c
struct ClientSlotInUseMessage {
	uint8_t msg_type; /* must be 0xF7 */
}
```

This message occurs as respond to the HELLO message.
It signals that the request slot is already in use.
After this message the server closes the connection.

## CLIENT_SLOTS_EXHAUSTED 0xF8

```c
struct ClientSlotsExhausted {
	uint8_t msg_type; /* must be 0xF7 */
}
```

This message occurs as respond to the HELLO message or direct after
connection.
It signals that the server has no slots anymore.
After this message the server closes the connection.


## QUIT

```c
struct QuitMessage {
	uint8_t msg_type; /* must be 0xF9 */
}
```

The client may send this message to close the connection.
This should close the uinput device on the server.
The server closes than the socket.

## DEVICE_NOT_ALLOWED

```c
struct DeviceNotAllowedMessage {
	uint8_t msg_type; /* must be 0xFA */
}
```

This messages occurs after the SETUP END message.
It signals that the request device type is not allowed on the server.
After this message the server closes the connection.

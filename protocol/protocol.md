Network gamepads protocol documentation.

This document describes version 5 (0x05) of the protocol.

# Security considerations

* Do not use this protocol to send your keyboard over the internet.
	The connection is not secured in any way (except for a plaintext password)
* Use it only in environments you control.
* Don't type passwords.
* Don't use a server password that you use on other services.

In order to (securely) send keyboard input to a remote X session, use
```bash
	ssh -X <server> x2x -to :0 -nomouse
```
# Introduction

The protocol consists of a set of messages, each starting
with a one byte message type and followed by zero or more data
bytes, the layout of which depends on the message type.

# Overview of message types
| Message name           | Message type byte |
|------------------------|------------|
| HELLO                  | 0x01       |
| PASSWORD               | 0x02       |
| ABSINFO                | 0x03       |
| DEVICE                 | 0x04       |
| SETUP_END              | 0x05       |
| REQUEST_EVENT          | 0x06       |
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

# Client Messages

The client initiates a TCP connection to the server and begins the exchange by sending a
`HELLO` message.

All messages are described in detail below.

## The `HELLO` message

```c
struct HelloMessage {
	uint8_t msg_type; /* must be 0x01 */
	uint8_t version; /* must be PROTOCOL_VERSION (currently 0x05) */
	uint8_t slot; /* The client slot requested */
}
```

The HELLO message must be the first message sent on a newly created connection.

Its data part contains

* (1 Byte) Protocol version
	This field is used to negotiate the feature set to be supported
* (1 Byte) Requested client slot
	The client slot to be acquired for this client.
	Reconnecting to a client slot previously occupied keeps the
	evdev device intact. A client slot may be assigned to only
	one client. A special value of `0` indicates that the server
	should randomly assign the client a slot.


### Possible responses

* `VERSION_MISMATCH`
	The server does not support the protocol version indicated by the client
* `INVALID_MESSAGE`
	The first message received on this connection was not `HELLO`
* `INVALID_CLIENT_SLOT`
	An invalid slot number was passed
* `CLIENT_SLOT_IN_USE`
	The requested slot was already occupied
* `CLIENT_SLOTS_EXHAUSTED`
	The server is at it's client limit and can not accept further connections
	until one or more clients disconnect
* `SETUP_REQUIRED`
	Continue by sending a `DEVICE` message
* `PASSWORD_REQUIRED`
	Continue by sending a `PASSWORD` message
* `SUCCESS`
	The client may now send `DATA` messages

### Example
    Client -> Server
    0x01 0x01 0x00

* Message type: `HELLO`
* Version: 0x01
* Client slot: auto-assign

## The `PASSWORD` message

```c
struct PasswordMessage {
	uint8_t msg_type; /* must be 0x02 */
	uint8_t length; /* Password length */
	uint8_t password[length]; /* Password data */
}
```

This message must be sent in response to a `PASSWORD_REQUIRED` message from the server.

The data part consists of

* (1 Byte) Password length
	The length (excluding any terminating characters) of the connection password
* (`length` Bytes) The password
	Note that the password need not be ASCII, but ease-of-use recommends it

### Possible responses

* `INVALID_PASSWORD`
	The given password was not correct. The server will terminate the connection.
* `SETUP_REQUIRED`
	Continue by sending a `DEVICE` message
* `SUCCESS`
	The client may now continue by sending `DATA` messages

### Example

    Client -> Server
    0x02 0x04 0x48 0x45 0x4C 0x4F

* Message type: `PASSWORD`
* Length: 4 Bytes
* Password: `HELO`

## The `DEVICE` message

```c
struct DeviceMessage {
	uint8_t msg_type; /* Must be 0x04 */
	uint8_t length; /* length of name field */
	struct input_id ids; /* see input.h */
	char name[length]; /* the name to report for the device */
}

```

This message must be sent in response to a `SETUP_REQUIRED` message from the server.
It contains data used for creating the input device on the server.

The data part is as follows

* (1 Byte) Name length
	The length of the data in the `name` field. Must not exceed
	`UINPUT_MAX_NAME_SIZE` bytes
* (??? Bytes) Input device data structure
	Extended information about the device to emulate on the server
	`TODO Extend this`
* (`length` Bytes) Device name
	The name that will be displayed for this X input device

The `DEVICE` message may optionally be followed by one or more `ABSINFO` messages.

### Possible responses

The server will not issue a response until it receives a `SETUP_END` message.

### Example

`TODO`

## The `ABSINFO` message

```c
struct ABSInfoMessage {
	uint8_t msg_type; /* Must be 0x03 */
	uint8_t axis; /* must be smaller than MSG_MAX (see linux/input-event-codes.h)
	struct input_absinfo info; /* see linux/input.h */
}
```

This message contains informations about the extents and capabilities of an absolute axis.
The client must send this message for every absolute axis of the device it wants to use.

The data part consists of

* (1 Byte) Axis identifier
	See linux/input.h for all available axes (ABS_*)
* (??? Bytes) `absinfo` structure (see `linux/input.h`)

### Possible responses

The server will not issue a response until it receives a `SETUP_END` message.

### Example

`TODO`

## The `REQUEST_EVENT` message

```c
struct RequestEventMessage {
	uint8_t msg_type; /* Must be 0x06 */
	uint16_t type;
	uint16_t code;
}
```

This message requests for a key scan code or axis to be enabled on the server device.
The server may silently ignore this message according to configured black-/whitelists.
The client must send this message for each type/code combination it will generate.

The data part consists of

* (2 Bytes) Type identifier (e.g. `EV_KEY`), as defined in `linux/input.h`
* (2 Bytes) Code identifier (e.g. `KEY_A`), as defined in `linux/input.h`

### Possible responses

The server will not issue a response until it receives a `SETUP_END` message.

### Example

    Client -> Server
    0x06 0x01 0x00 0x01 0x00

* Message type: `REQUEST_EVENT`
* Type: 0x00 01 (`EV_KEY`)
* Code: 0x00 01 (`KEY_ESC`)

## The `SETUP_END` message

```c
struct SetupEndMessage {
	uint8_t msg_type; /* must be 0x05 */
}
```

Indicates to the server that the client is done configuring the remote device and requests
to send data.

### Possible responses

* `SUCCESS`
	The client may now send `DATA` messages
* ???

## The `DATA` message

```c
struct DataMessage {
	uint8_t msg_type; /* must be 0x10 */
	uint16_t type;
	uint16_t code;
	int32_t value;
}
```

Sends an input event to the server for injection. The data may be filtered on the server
according to configured black/whitelists.

The data part consists of

* (2 Bytes) Event type (e.g. `EV_KEY`), defined in `linux/input.h`
* (2 Bytes) Event code (e.g. `KEY_ESC`), defined in `linux/input.h`
* (4 Bytes) Event value

The data part layout closely mirrors the `struct input_event` from `linux/input.h`.

### Possible responses

* `INVALID_MESSAGE`
	Encountered when sending `DATA` message while the device is not yet configured
* Nothing
	When the operation has succeeded

### Example

`TODO`

## The `QUIT` message

```c
struct QuitMessage {
	uint8_t msg_type; /* must be 0xF9 */
}
```

The client may send this message to close the connection.
The server will destroy the input node on the server and terminate the client connection.

### Possible responses

Connection termination

# Server responses

The server responds to commands by the clients using the following response
messages

## The `SUCCESS` response

```c
struct SuccessMessage {
	uint8_t msg_type; /* must be 0xF0 */
	uint8_t slot;
}
```

The client has successfully negotiated or been assigned a device and may now
send `DATA` messages.

The data part contains

* (1 Byte) Client slot
	The slot the client has been assigned.
	Should the connection be terminated during normal operation,
	the client may reconnect and be assigned the same input device
	as before by specifying the slot it had in the previous connection
	in the `HELLO` message.

## The `VERSION_MISMATCH` response

```c
struct VersionMismatchMessage {
	uint8_t msg_type; /* must be 0xF1 */
	uint8_t version; /* server protocol version */
}
```

Sent to indicate that the server does not support the protocol version indicated
by the clients `HELLO` message. The server will terminate the connection after sending
this response.

The data part contains

* (1 Byte) Server version
	The protocol version spoken by the server

## The `INVALID_PASSWORD` response

```c
struct InvalidPasswordMessage {
	uint8_t msg_type; /* must be 0xF2 */
}
```

The password used by the client was not recognized by the server.

## The `INVALID_CLIENT_SLOT` response

```c
struct InvalidClientSlotMessage {
	uint8_t msg_type; /* must be 0xF3 */
}
```

The slot requested by the client in the `HELLO` message was not valid.
This may occur when the number of client slots is limited on the server.
The server will terminate the connection after sending this message.

## The `INVALID_MESSAGE` response

```c
struct InvalidMessage {
	uint8_t msg_type; /* must be 0xF4 */
}
```

The server may send this message at any time to indicate an unexpected command
by the client.

The server MAY terminate the connection after sending this message.

## The `PASSWORD_REQUIRED` response

```c
struct PasswordRequiredMessage {
	uint8_t msg_type; /* must be 0xF5 */
}
```

This message indicates to the client that a `PASSWORD` command is required to proceed.

## The `SETUP_REQUIRED` response

```c
struct SetupRequiredMessage {
	uint8_t msg_type; /* must be 0xF6 */
}
```

This message indicates to the client that a `DEVICE` command is required to proceed.

This response may also be sent by the client when device setup has already successfully
completed, prompting the server to destroy the old input device and wait for a new
`DEVICE`/`ABSINFO` exchange after answering with this same message.

## The `CLIENT_SLOT_IN_USE` response

```c
struct ClientSlotInUseMessage {
	uint8_t msg_type; /* must be 0xF7 */
}
```

The slot requested by the clients `HELLO` message is already in use.
The server may terminate the connection after sending this response.

## The `CLIENT_SLOTS_EXHAUSTED` response

```c
struct ClientSlotsExhausted {
	uint8_t msg_type; /* must be 0xF8 */
}
```

Indicates to the client that the server can not accept any new clients at this time.
The server will terminate the connection after sending this response.

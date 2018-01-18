# network-gamepads - Pass input devices over a network connection

This project, split into a server part and a client part, allows you
to use a keyboard, mouse, gamepad or other input device connected physically to 
one linux computer as if it were connected to another one via the network.

Originally written to support big-screen playing of single-screen multiplayer games,
the project has developed to be generic enough to support just about any type of
input device.

# Project components

## The server

The server allows up to 8 clients to connect and creates a new virtual input interface
for each, through which input events will be passed to the system. This component
is intended to be run on the server hosting the game. It may be necessary to run the
server component as `root` or add the user running it to the `input` group.

When installed, this component will be available as `input-server`

## The client

The client grabs a single input device on the local computer and tries to stream all
events originating from it to the server. The client tries to grab the input data exclusively,
not allowing it to pass to local input processing. Using the client on your primary input device
is not recommended. It may be necessary to run the client component as `root` or add the user 
running it to the `input` group.

When installed, this component will be available as `input-client`

# Building & setup

## Build prerequisites

A working C compiler and (GNU) make, in addition to the Linux `uinput` headers (`linux/uinput.h`),
which is included in the package `linux-libc-dev` in Debian. To build the server [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/) is needed.

## Build

After installing the prerequisite packages, building by running `make` should work without problems.

## Installation

By running `make install`, both components are installed to `/usr/local/bin`.

To install only one component, run `make install-server` or `make install-client`, respectively.
Neither is required though, running the tools directly from the build directory is fine.

## Usage

### Server

Run the server by starting `input-server`. By default, the server listens on `::` port `9292`.
These settings can be overridden by specifying the environment variables `SERVER_HOST` and `SERVER_PORT` or the corresponding command line arguments.

To connect to the server, users need to provide a password. The default password is `foobar`.
It may be overridden by specifying the environment variable `SERVER_PW` or the corresponding command line argument.

To limit the types of keys/axes a client may use on the server, black- and whitelists are used.
These contain lines space-separated `type.code` pairs and either allow only these events (whitelists) or everything
except these (blacklist). Instead of `type.*` enables or disables all events of the given type. Lines beginning with `#` are comments. Example lists for the most used types can be found in [acls/](acls/).
The default configuration allows all events.


### Client

To stream an input device connected to your local computer, run `input-client [-h <host>] [-n <name>] [input device node]`.
Optionally, you can specify some or all of the following parameters via environment variables or command line arguments:

* `SERVER_HOST`: The host to connect to (Default: `::`)
* `SERVER_PORT`: The port to connect to (Default `9292`)
* `SERVER_PW`: The password required for the server (Default `foobar`)

The host (`-h`) argument specifies the server to connect to and will override a given `SERVER_HOST` environment variable.

The name (`-n`) argument can be used to specify an optional name used on the server, eg. for mapping devices to players or button mapping profiles. This name is also used as the `evdev` device name on the server.

The last option, the input device node, is optional. If you do not supply one, the client will ask you which device you want to stream.

While the client is running (and after it has successfully connected), input events generated should take effect
on the computer running the server.

# Background

## Realization

The client component first tries to open the supplied device node for exclusive access, preventing other
clients such as X11/xorg from also reacting to the input (Hence why starting the client on primary input devices is
not really advisable). After opening the device node, the client reads `struct input_event` data objects and sends
them to the server.

The server uses the `uinput` kernel facility to create virtual device nodes and inject the streamed events
(after filtering them somewhat) into them. The events are then processed by the X server on the remote machine
and treated as if the input devices were attached directly.

## Protocol

A detailed description of the protocol may be found in [protocol/protocol.md](protocol/protocol.md)

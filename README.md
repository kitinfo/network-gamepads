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

In addition to a working C compiler and make, the package `libevdev-dev` (debian) is required
to build the project.

## Build

After installing the prerequisite packages, building by running `make` should work without problems.

## Installation

By running `make install`, both components are installed to `/usr/bin`.

To install only one component, run `make install-server` or `make install-client`, respectively.

## Usage

### Server

Run the server by starting `input-server`. By default, the server listens on `::` port `9292`.
These settings can be overridden by specifying the environment variables `SERVER_HOST` and `SERVER_PORT`.

To connect to the server, users need to provide a password. The default password is `foobar`.
It may be overridden by specifying the environment variable `SERVER_PW`.

### Client

First, find out the device node you will want to stream to the server in the `/dev/input/` directory.
The directory `/dev/input/by-id/` has symlinks that are named in a pretty readable way.

To test whether a device node is the one you want, run `cat /dev/input/path/to/node` and press some keys.
Each keypress should print some seemingly random data.

Having found your device node, run `input-client /dev/input/path/to/input/node`, while specifying the following
parameters via environment variables:

* `SERVER_HOST`: The host to connect to (Default: `::`)
* `SERVER_PORT`: The port to connect to (Default `9292`)
* `SERVER_PW`: The password required for the server (Default `foobar`)

While the client is running (and after it has successfully connected), input events generated should take effect
on the computer running the server.

# Background

## Realization

TBD

## Protocol

TBD

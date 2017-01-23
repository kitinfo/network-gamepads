-- network gamepads protocol
print("Hello World")

-- do not modify this table
local debug_level = {
	DISABLED = 0,
	LEVEL_1 = 1,
	LEVEL_2 = 2
}

-- set debug level
local DEBUG = debug_level.LEVEL_1

local default_settings = {
	debug_level  = DEBUG,
	enabled      = true,
	port         = 9292,
	max_msg_len  = 128,
}

local dprint = function() end
local dprint2 = function() end
local function resetDebugLevel()
	if default_settings.debug_level > debug_level.DISABLED then
		dprint = function(...)
			info(table.concat({"Lua: ", ...}, " "))
		end

		if default_settings.debug_level > debug_level.LEVEL_1 then
			dprint2 = dprint
		end
	else
		dprint = function() end
		dprint2 = dprint
	end
end

resetDebugLevel()

-- declare
local ngamepads_proto = Proto("ngamepads", "Network Gamepads Protocol")

local function makeValString(enumTable)
	local t = {}
	for name, num in pairs(enumTable) do
		t[num] = name
	end
	return t
end

local msgtype = {
	HELLO                  = 0x01,
	PASSWORD               = 0x02,
	ABSINFO                = 0x03,
	DEVICE                 = 0x04,
	SETUP_END              = 0x05,
	DATA                   = 0x10,
	SUCCESS                = 0xF0,
	VERSION_MISMATCH       = 0xF1,
	INVALID_PASSWORD       = 0xF2,
	INVALID_CLIENT_SLOT    = 0xF3,
	INVALID                = 0xF4,
	PASSWORD_REQUIRED      = 0xF5,
	SETUP_REQUIRED         = 0xF6,
	CLIENT_SLOT_IN_USE     = 0xF7,
	CLIENT_SLOTS_EXHAUSTED = 0xF8,
	QUIT                   = 0xF9
}

local axismap = {
	ABS_X        = 0x00,
	ABS_Y        = 0x01,
	ABS_Z        = 0x02,
	ABS_RX       = 0x03,
	ABS_RY       = 0x04,
	ABS_RZ       = 0x05,
	ABS_THROTTLE = 0x06,
	ABS_RUDDER   = 0x07,
	ABS_WHEEL    = 0x08,
	ABS_GAS      = 0x09,
	ABS_BRAKE    = 0x0a,
	ABS_HAT0X    = 0x10,
	ABS_HAT0Y    = 0x11,
	ABS_HAT1X    = 0x12,
	ABS_HAT1Y    = 0x13,
	ABS_HAT2X    = 0x14,
	ABS_HAT2Y    = 0x15,
	ABS_HAT3X    = 0x16,
	ABS_HAT3Y    = 0x17,
	ABS_PRESSURE = 0x18,
	ABS_DISTANCE = 0x19,
	ABS_TILT_X   = 0x1a,
	ABS_TILT_Y   = 0x1b,
}

local devtype = {
	UNKOWN   = 0,
	GAMEPAD  = 1,
	MOUSE    = 2,
	KEYBOARD = 3
}

local msgtype_valstr = makeValString(msgtype)
local axis_valstr = makeValString(axismap)
local devtype_valstr = makeValString(devtype)

local hdr_fields = {
	version  = ProtoField.uint8("ng.version", "Version", base.DEC),
	msg_type = ProtoField.uint8("ng.msg_type", "Message Type", base.HEX, msgtype_valstr),
	length   = ProtoField.uint8("ng.length", "Length", base.DEC),
	slot     = ProtoField.uint8("ng.slot", "Slot", base.DEC),
	axis     = ProtoField.uint8("ng.axis", "Axis", base.DEC, axis_valstr),
	minimum  = ProtoField.int32("ng.absinfo.minimum", "Minimum", base.DEC),
	maximum  = ProtoField.int32("ng.absinfo.maximum", "Maximum", base.DEC),
	value    = ProtoField.int32("ng.absinfo.value", "Value", base.DEC),
	fuzz     = ProtoField.int32("ng.absinfo.fuzz", "Fuzz", base.DEC),
	flat     = ProtoField.int32("ng.absinfo.flat", "Flat", base.DEC),
	resolution = ProtoField.int32("ng.absinfo.resolution", "Resolution", base.DEC),
	devtype    = ProtoField.uint8("ng.type", "Type", base.DEC, devtype_valstr),
	name       = ProtoField.string("ng.name", "Name", base.STRING),
	password   = ProtoField.string("ng.password", "Password", base.STRING),
	vendor     = ProtoField.uint16("ng.id.vendor", "Vendor", base.HEX),
	bustype    = ProtoField.uint16("ng.id.bustype", "Bustype", base.HEX),
	product    = ProtoField.uint16("ng.id.product", "Product", base.HEX),
	idversion  = ProtoField.uint16("ng.id.version", "Version", base.HEX),
	event_time_sec = ProtoField.uint64("ng.event.time.sec", "Time (sec)", base.DEC),
	event_time_usec = ProtoField.uint64("ng.event.time.usec", "Time (usec)", base.DEC),
	event_type = ProtoField.uint16("ng.event.type", "Type", base.HEX),
	event_code = ProtoField.uint16("ng.event.code", "Code", base.HEX),
	event_value= ProtoField.int32("ng.event.value", "Value", base.DEC)
}

ngamepads_proto.fields = hdr_fields

dprint2("ngamepads_proto ProtoFields registered")

local tvbs = {}

function ngamepads_proto.init()
	-- reset the save Tvbs
	tvbs = {}
end

-- minimum size of a message
local NGAMEPADS_MSG_MIN_LEN = 1

-- some forward "declarations" of helper functions we use in the dissector
local createSllTvb, dissectNGamepads, checkNGamepadsLength

-- this holds the plain "data" Dissector, in case we can't dissect it as Netlink
local data = Dissector.get("data")

-- create a function to dissect it
function ngamepads_proto.dissector(tvbuf, pktinfo, root)
	dprint2("ngamepads_proto.dissector called")
	-- reset the save Tvbs
	tvbs = {}

	local pktlen = tvbuf:len()

	local bytes_consumed = 0

	while bytes_consumed < pktlen do
		local result = dissectNGamepads(tvbuf, pktinfo, root, bytes_consumed)

		if result > 0 then
			bytes_consumed = bytes_consumed + result
		elseif result == 0 then
			return 0
		else
			pktinfo.desegment_offset = bytes_consumed

			-- invert the negative result so it's a positive number
			result = -result

			pktinfo.desegment_len = result

			return pktlen
		end
	end

	return bytes_consumed
end


dissectNGamepads = function(tvbuf, pktinfo, root, offset)
	dprint2("NGamepads dissect function called")

	local length_val = checkNGamepadsLength(tvbuf, offset)

	if length_val <= 0 then
		dprint2("Ngamepads length check failed.")
		return length_val
	end

	pktinfo.cols.protocol:set("NGamepads")

	if string.find(tostring(pktinfo.cols.info), "^NGamepads") == nil then
		pktinfo.cols.info:set("Ngamepads")
	end

	local tree = root:add(ngamepads_proto, tvbuf:range(offset, length_val))

	-- msg_type
	local msg_type_tvbr = tvbuf:range(offset, 1)
	local msg_type_val = msg_type_tvbr:uint()
	tree:add(hdr_fields.msg_type, msg_type_tvbr)

	if msg_type_val == msgtype.HELLO then
		-- version
		tree:add(hdr_fields.version, tvbuf:range(offset + 1, 1))
		tree:add(hdr_fields.slot, tvbuf:range(offset + 2, 1))
	elseif msg_type_val == msgtype.PASSWORD then
		local len = tvbuf:range(offset + 1, 1)
		tree:add(hdr_fields.length, len)
		tree:add(hdr_fields.password, tvbuf:range(offset + 2, len:uint()))
	elseif msg_type_val == msgtype.ABSINFO then
		tree:add(hdr_fields.axis, tvbuf:range(offset + 1, 1))
		tree:add(hdr_fields.value, tvbuf:range(offset + 2, 4))
		tree:add(hdr_fields.minimum, tvbuf:range(offset + 6, 4))
		tree:add(hdr_fields.maximum, tvbuf:range(offset + 10, 4))
		tree:add(hdr_fields.fuzz, tvbuf:range(offset + 14, 4))
		tree:add(hdr_fields.flat, tvbuf:range(offset + 18, 4))
		tree:add(hdr_fields.resolution, tvbuf:range(offset + 22, 4))
	elseif msg_type_val == msgtype.DEVICE then
		local len = tvbuf:range(offset + 1, 1)
		tree:add(hdr_fields.length, len)
		tree:add(hdr_fields.devtype, tvbuf:range(offset + 2, 1))
		tree:add(hdr_fields.bustype, tvbuf:range(offset + 3, 2))
		tree:add(hdr_fields.vendor, tvbuf:range(offset + 5, 2))
		tree:add(hdr_fields.product, tvbuf:range(offset + 7, 2))
		tree:add(hdr_fields.idversion, tvbuf:range(offset + 9, 2))
		tree:add(hdr_fields.name, tvbuf:range(offset + 11, len:uint()))
	elseif msg_type_val == msgtype.DATA then
		tree:add(hdr_fields.event_time_sec, tvbuf:range(offset + 1, 8))
		tree:add(hdr_fields.event_time_usec, tvbuf:range(offset + 9, 8))
		tree:add(hdr_fields.event_type, tvbuf:range(offset + 17, 2))
		tree:add(hdr_fields.event_code, tvbuf:range(offset + 19, 2))
		tree:add(hdr_fields.event_value, tvbuf:range(offset + 21, 4))
	end

	return length_val
end

checkNGamepadsLength = function(tvbuf, offset)
	local msglen = tvbuf:len() - offset

	dprint2("msglen is " .. msglen)

	if msglen ~= tvbuf:reported_length_remaining(offset) then
		dprint2("Captured packet was shorter than original, can't reassamble")
		return 0
	end

	if msglen < 1 then
		dprintf2("Need more bytes to figure out msgtype field")
		return -DESEGMENT_ONE_MORE_SEGMENT
	end

	local msgtype_val = tvbuf:range(offset, 1):uint()

	if msgtype_val == msgtype.HELLO then
		return 3
	elseif msgtype_val == msgtype.PASSWORD then
		if msglen < 2 then
			return -DESEGMENT_ONE_MORE_SEGMENT
		else
			return tvbuf:range(offset + 1, 1):uint() + 2
		end
	elseif msgtype_val == msgtype.ABSINFO then
		return 28
	elseif msgtype_val == msgtype.DEVICE then
		if msglen < 2 then
			return -DESEGMENT_ONE_MORE_SEGMENT
		else
			return tvbuf:range(offset + 1, 1):uint() + 12
		end
	elseif msgtype_val == msgtype.DATA then
		return 32
	elseif msgtype_val == msgtype.VERSION_MISMATCH then
		return 2
	elseif msgtype_val == msgtype.SUCCESS then
		return 2
	elseif msgtype_val == msgtype.SETUP_END then
		return 1
	elseif msgtype_val == msgtype.INVALID_PASSWORD then
		return 1
	elseif msgtype_val == msgtype.SETUP_REQUIRED then
		return 1
	elseif msgtype_val == msgtype.CLIENT_SLOT_IN_USE then
		return 1
	elseif msgtype_val == msgtype.CLIENT_SLOTS_EXHAUSTED then
		return 1
	elseif msgtype_val == msgtype.QUIT then
		return 1
	else
		dprint2("unkown msg_type")
		return 0
	end
end

local function enableDissector()
	DissectorTable.get("tcp.port"):add(default_settings.port, ngamepads_proto)
end
enableDissector()

local function disableDissector()
	DissectorTable.get("tcp.port"):remove(default_settings.port, ngamepads_proto)
end

local debug_pref_enum = {
	{ 1, "Disabled", debug_level.DISABLED },
	{ 2, "Level 1", debug_level.LEVEL_1 },
	{ 3, "Level 2", debug_level.LEVEL_2 }
}

ngamepads_proto.prefs.enabled = Pref.bool("Dissector enabled", default_settings.enabled, "Whether the NGampads dissector is enabled or not")
ngamepads_proto.prefs.debug   = Pref.enum("Debug", default_settings.debug_level, "The debug printing level", debug_pref_enum)

function ngamepads_proto.prefs_changed()
	dprint2("prefs_changed called")

	default_settings.debug_level = ngamepads_proto.prefs.debug
	resetDebugLevel()

	if default_settings.enabled ~= ngamepads_proto.prefs.enabled then
		default_settings.enabled = ngamepads_proto.prefs.enabled
		if default_settings.enabled then
			enableDissector()
		else
			disableDissector()
		end

		reload()
	end
end

dprint2("pcapfile Prefs registered")

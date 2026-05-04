--[[

	 ___											
	| _ )  											
	| _ \ 											
	|___/eemo's 									
	. . . . . . test_emulator 						
	. . . . . . Sitool Test Device emulator 		
	. . . . . . nflatrea@mailo.com <Noë Flatreaud>

	Protocol: [0x02] [CMD_ID] [PARAMS...]

		0x00 STATUS    -> 0x00 (OK)
		0x01 VERSION   -> 0x02 0x01 0x05  (major.minor.patch)
		0x03 GET_INFO  -> 0x01 0x00 0x00 0x00 "TESTINGA"
		0x05 GET_DATA  -> 0x04 0x0A 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF
		0x10 ECHO      -> echoes remaining bytes
		0x20 SET_LED   -> 0x00 (OK)

	Les commandes sont définis en LUA à partir du protocol au dessus,
	chaque command retourne un certain payload, qui peut à son tour
	être parsé et afficher joliment

	Usage:

	$ ./test/test_emulator
	$ ./sitool /dev/pts/2
	pts/2> use example
	pts/2> get_info

	TX>> 02 00
	RX<< 00

	Output : OK(00)

]]

local H = {}
H.commands  = {}
H.callbacks = {}

local HEADER = 0x02

local function build(cmd_id, ...)
    local parts = {string.format("%02X %02X", HEADER, cmd_id)}
    for _, v in ipairs({...}) do
        parts[#parts + 1] = v
    end
    return table.concat(parts, " ")
end

local STATUS_NAMES = {
    [0x00] = "OK",
    [0x01] = "ERR_UNKNOWN_CMD",
    [0x02] = "ERR_BAD_PARAM",
    [0xFF] = "ERR_INTERNAL",
}

local function status_str(code)
    return STATUS_NAMES[code] or string.format("UNKNOWN(0x%02X)", code)
end

-- status
H.commands.status = {
    name  = "status",
    usage = "status",
    help  = "Query device status (CMD 0x00)",
    run   = function(_)
        local _, raw = sitool.send(build(0x00))
        if not raw then
            print("[!] No response")
            return
        end
        local code = string.byte(raw, 1)
        printf("Status: %s\n", status_str(code))
    end
}

-- version
H.commands.version = {
    name  = "version",
    usage = "version",
    help  = "Query firmware version (CMD 0x01)",
    run   = function(_)
        local _, raw = sitool.send(build(0x01))
        if not raw then
            print("[!] No response")
            return
        end
        if #raw >= 3 then
            printf("Firmware: v%d.%d.%d\n",
                   string.byte(raw, 1),
                   string.byte(raw, 2),
                   string.byte(raw, 3))
        else
            printf("[!] Unexpected response length: %d\n", #raw)
        end
    end
}

-- get_info
H.commands.get_info = {
    name  = "get_info",
    usage = "get_info",
    help  = "Read device info string (CMD 0x03)",
    run   = function(_)
        local _, raw = sitool.send(build(0x03))
        if not raw then
            print("[!] No response")
            return
        end
        if #raw < 4 then
            printf("[!] Unexpected response length: %d\n", #raw)
            return
        end
        local flags = string.byte(raw, 1)
        local r1    = string.byte(raw, 2)
        local r2    = string.byte(raw, 3)
        local r3    = string.byte(raw, 4)
        local name  = #raw > 4 and raw:sub(5) or ""
        printf("Info:\n")
        printf("  Flags    : 0x%02X\n", flags)
        printf("  Reserved : %02X %02X %02X\n", r1, r2, r3)
        printf("  Name     : %s\n", name)
    end
}

-- get_data
H.commands.get_data = {
    name  = "get_data",
    usage = "get_data",
    help  = "Read sensor/data payload (CMD 0x05)",
    run   = function(_)
        local _, raw = sitool.send(build(0x05))
        if not raw then
            print("[!] No response")
            return
        end
        if #raw < 2 then
            printf("[!] Unexpected response length: %d\n", #raw)
            return
        end
        local dtype = string.byte(raw, 1)
        local dlen  = string.byte(raw, 2)
        local payload = raw:sub(3)
        printf("Data:\n")
        printf("  Type     : 0x%02X\n", dtype)
        printf("  Length   : %d\n", dlen)
        printf("  Payload  : %s\n", sitool.utils.btoh(payload))
        if #payload > 0 then
            printf("  Hexdump:\n%s", sitool.utils.hex(payload))
        end
    end
}

-- echo
H.commands.echo = {
    name  = "echo",
    usage = "echo <hex bytes>",
    help  = "Echo test: send bytes, expect them back (CMD 0x10)",
    run   = function(args)
        if not args or args == "" then
            print("usage: echo <hex bytes>")
            return
        end
        local _, raw = sitool.send(build(0x10, args))
        if not raw then
            print("[!] No response")
            return
        end
        local sent = sitool.utils.htob(args)
        if raw == sent then
            printf("Echo OK (%d bytes match)\n", #raw)
        else
            printf("Echo MISMATCH:\n")
            printf("  Sent : %s\n", args)
            printf("  Recv : %s\n", sitool.utils.btoh(raw))
        end
    end
}

-- set_led
H.commands.set_led = {
    name  = "set_led",
    usage = "set_led <state>",
    help  = "Set LED state: 00=off, 01=on, 02=blink (CMD 0x20)",
    run   = function(args)
        if not args or args == "" then
            print("usage: set_led <state>")
            print("  00 = off")
            print("  01 = on")
            print("  02 = blink")
            return
        end
        local _, raw = sitool.send(build(0x20, args))
        if not raw then
            print("[!] No response")
            return
        end
        local code = string.byte(raw, 1)
        printf("set_led: %s\n", status_str(code))
    end
}

-- on_recv dissector
local CMD_NAMES = {
    [0x00] = "STATUS",
    [0x01] = "VERSION",
    [0x03] = "GET_INFO",
    [0x05] = "GET_DATA",
    [0x10] = "ECHO",
    [0x20] = "SET_LED",
}

H.callbacks.on_recv = function(raw)
    if #raw < 1 then return end
    -- try to identify by first byte patterns
    local b0 = string.byte(raw, 1)
    local hex = sitool.utils.btoh(raw)
    printf("[dissect] len=%d data=%s\n", #raw, hex)
end

H.callbacks.on_load = function()
    print("[example] Handler loaded")
    print("[example] Type \"help\" for help")
end

H.callbacks.on_unload = function()
    print("[example] Handler unloaded")
end

return H

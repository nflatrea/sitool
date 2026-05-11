--[[

     ___
    | _ )
    | _ \
    |___/eemo's
    . . . . . . modbus
    . . . . . . Modbus RTU master handler for sitool

    Modbus RTU frame:

        [ADDR:1] [FUNC:1] [DATA:N] [CRC:2 LE]

        - Address       : 1 byte slave address (1-247)
        - Function code : 1 byte
        - Data          : variable, depends on function
        - CRC-16        : 2 bytes, little-endian (LSB first)

    Supported function codes:

        01  Read Coils (Digital Outputs)
        02  Read Discrete Inputs
        03  Read Holding Registers
        04  Read Input Registers
        05  Force Single Coil
        06  Preset Single Register
        08  Loopback Diagnostic
        16  Preset Multiple Registers
        17  Report Device ID

    Usage:

        $ ./sitool -p /dev/ttyUSB0 -b 9600
        sitool> set parity E
        sitool> set databits 8
        sitool> set stopbits 1
        sitool> use modbus
        sitool> slave 1
        sitool> read_hr 0 10
        sitool> write_sr 100 0x1234

]]

local H = {}
H.commands  = {}
H.callbacks = {}


local _slave_addr = 1

local function crc16(data)
    local crc = 0xFFFF
    for i = 1, #data do
        crc = crc ~ string.byte(data, i)
        for _ = 1, 8 do
            if (crc % 2) == 1 then
                crc = (crc >> 1) ~ 0xA001
            else
                crc = crc >> 1
            end
        end
    end
    return crc
end

local function append_crc(data)
    local c = crc16(data)
    return data .. string.char(c % 256, math.floor(c / 256))
end

local function check_crc(data)
    if #data < 3 then return false end
    local body = data:sub(1, -3)
    local rx_lo = string.byte(data, #data - 1)
    local rx_hi = string.byte(data, #data)
    local rx_crc = rx_lo + rx_hi * 256
    return crc16(body) == rx_crc
end

local function build_hex(...)
    local parts = {}
    for _, v in ipairs({...}) do
        if type(v) == "number" then
            parts[#parts + 1] = string.format("%02X", v % 256)
        elseif type(v) == "string" then
            parts[#parts + 1] = v
        end
    end
    return table.concat(parts, " ")
end

local function mb_request(func_code, payload_bytes)
    -- build raw binary for CRC calculation
    local raw_parts = { string.char(_slave_addr), string.char(func_code) }
    if payload_bytes then
        raw_parts[#raw_parts + 1] = payload_bytes
    end
    local raw_body = table.concat(raw_parts)
    local raw_with_crc = append_crc(raw_body)

    -- convert to hex string for sitool.send()
    local hex_parts = {}
    for i = 1, #raw_with_crc do
        hex_parts[#hex_parts + 1] = string.format("%02X", string.byte(raw_with_crc, i))
    end
    local hex_str = table.concat(hex_parts, " ")

    local _, raw = sitool.send(hex_str)
    return raw
end

local EXCEPTION_NAMES = {
    [0x01] = "ILLEGAL FUNCTION",
    [0x02] = "ILLEGAL DATA ADDRESS",
    [0x03] = "ILLEGAL DATA VALUE",
    [0x04] = "SLAVE DEVICE FAILURE",
    [0x05] = "ACKNOWLEDGE",
    [0x06] = "SLAVE DEVICE BUSY",
    [0x07] = "NEGATIVE ACKNOWLEDGE",
    [0x08] = "MEMORY PARITY ERROR",
}

local function check_response(raw, expected_func)
    if not raw then
        print("[!] No response from slave")
        return nil
    end

    if #raw < 3 then
        printf("[!] Response too short (%d bytes)\n", #raw)
        return nil
    end

    if not check_crc(raw) then
        printf("[!] CRC error in response\n")
        printf("[!] Raw: %s\n", sitool.utils.btoh(raw))
        return nil
    end

    local addr = string.byte(raw, 1)
    local func = string.byte(raw, 2)

    if addr ~= _slave_addr then
        printf("[!] Unexpected slave address: %d (expected %d)\n",
               addr, _slave_addr)
        return nil
    end

    -- exception response
    if func >= 0x80 then
        local exc_code = string.byte(raw, 3)
        local exc_name = EXCEPTION_NAMES[exc_code] or "UNKNOWN"
        printf("[!] Exception 0x%02X: %s\n", exc_code, exc_name)
        return nil
    end

    if func ~= expected_func then
        printf("[!] Unexpected function code: 0x%02X (expected 0x%02X)\n",
               func, expected_func)
        return nil
    end

    -- strip addr, func, and 2-byte CRC -> return data portion
    return raw:sub(3, #raw - 2)
end

local function u16(raw, offset)
    return string.byte(raw, offset) * 256 + string.byte(raw, offset + 1)
end

local function s16(raw, offset)
    local v = u16(raw, offset)
    if v >= 0x8000 then v = v - 0x10000 end
    return v
end

-- IEEE 754 float from two consecutive 16-bit registers (big-endian word order)
local function ieee_float(raw, offset)
    if #raw < offset + 3 then return nil end
    local b1 = string.byte(raw, offset)
    local b2 = string.byte(raw, offset + 1)
    local b3 = string.byte(raw, offset + 2)
    local b4 = string.byte(raw, offset + 3)

    local sign = (b1 >= 128) and -1 or 1
    local exp  = ((b1 % 128) * 2) + math.floor(b2 / 128)
    local mant = ((b2 % 128) * 65536) + (b3 * 256) + b4

    if exp == 0 and mant == 0 then return 0.0 end
    if exp == 0xFF then
        if mant ~= 0 then return 0/0 end -- NaN
        return sign * math.huge
    end

    return sign * math.ldexp(1 + mant / 8388608, exp - 127)
end

local function print_registers(start_reg, data, fmt)
    fmt = fmt or "hex"
    local byte_count = #data
    local num_regs = math.floor(byte_count / 2)

    printf("  Registers %d..%d (%d regs, %d bytes):\n",
           start_reg, start_reg + num_regs - 1, num_regs, byte_count)

    for i = 0, num_regs - 1 do
        local off = i * 2 + 1
        local reg = start_reg + i
        local val = u16(data, off)

        if fmt == "float" and i % 2 == 0 and i + 1 < num_regs then
            local fval = ieee_float(data, off)
            printf("    [%5d] 0x%04X 0x%04X  = %.6g (float)\n",
                   reg, val, u16(data, off + 2), fval or 0)
        elseif fmt == "float" and i % 2 == 1 then
            -- skip, already printed with previous register
        elseif fmt == "signed" then
            printf("    [%5d] 0x%04X  = %d (signed)\n", reg, val, s16(data, off))
        else
            printf("    [%5d] 0x%04X  = %d\n", reg, val, val)
        end
    end
end


-- slave <addr>
H.commands.slave = {
    name  = "slave",
    usage = "slave [addr]",
    help  = "Get/set Modbus slave address (1-247)",
    run   = function(args)
        if not args or args == "" then
            printf("Current slave address: %d\n", _slave_addr)
            return
        end
        local addr = tonumber(args)
        if not addr or addr < 1 or addr > 247 then
            print("[!] Address must be 1-247")
            return
        end
        _slave_addr = addr
        printf("Slave address set to %d\n", _slave_addr)
    end
}

-- read_coils <start> <count>  (FC 01)
H.commands.read_coils = {
    name  = "read_coils",
    usage = "read_coils <start> <count>",
    help  = "Read coils / digital outputs (FC 01)",
    run   = function(args)
        if not args or args == "" then
            print("usage: read_coils <start_addr> <count>")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end
        local start_addr = tonumber(parts[1]) or 0
        local count      = tonumber(parts[2]) or 1

        if count < 1 or count > 2000 then
            print("[!] Count must be 1-2000")
            return
        end

        local payload = string.char(
            math.floor(start_addr / 256), start_addr % 256,
            math.floor(count / 256), count % 256
        )
        local raw = mb_request(0x01, payload)
        local data = check_response(raw, 0x01)
        if not data then return end

        local byte_count = string.byte(data, 1)
        printf("  Coils %d..%d:\n", start_addr, start_addr + count - 1)

        local coil = start_addr
        for i = 2, 1 + byte_count do
            local b = string.byte(data, i)
            for bit = 0, 7 do
                if coil >= start_addr + count then break end
                local state = ((b >> bit) % 2) == 1
                printf("    [%5d] %s\n", coil, state and "ON" or "OFF")
                coil = coil + 1
            end
        end
    end
}

-- read_di <start> <count>  (FC 02)
H.commands.read_di = {
    name  = "read_di",
    usage = "read_di <start> <count>",
    help  = "Read discrete inputs (FC 02)",
    run   = function(args)
        if not args or args == "" then
            print("usage: read_di <start_addr> <count>")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end
        local start_addr = tonumber(parts[1]) or 0
        local count      = tonumber(parts[2]) or 1

        if count < 1 or count > 2000 then
            print("[!] Count must be 1-2000")
            return
        end

        local payload = string.char(
            math.floor(start_addr / 256), start_addr % 256,
            math.floor(count / 256), count % 256
        )
        local raw = mb_request(0x02, payload)
        local data = check_response(raw, 0x02)
        if not data then return end

        local byte_count = string.byte(data, 1)
        printf("  Discrete inputs %d..%d:\n", start_addr, start_addr + count - 1)

        local di = start_addr
        for i = 2, 1 + byte_count do
            local b = string.byte(data, i)
            for bit = 0, 7 do
                if di >= start_addr + count then break end
                local state = ((b >> bit) % 2) == 1
                printf("    [%5d] %s\n", di, state and "ON" or "OFF")
                di = di + 1
            end
        end
    end
}

-- read_hr <start> <count> [fmt]  (FC 03)
H.commands.read_hr = {
    name  = "read_hr",
    usage = "read_hr <start> <count> [hex|signed|float]",
    help  = "Read holding registers (FC 03)",
    run   = function(args)
        if not args or args == "" then
            print("usage: read_hr <start_reg> <count> [hex|signed|float]")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end

        local start_reg = tonumber(parts[1]) or 0
        local count     = tonumber(parts[2]) or 1
        local fmt       = parts[3] or "hex"

        if count < 1 or count > 125 then
            print("[!] Count must be 1-125")
            return
        end

        local payload = string.char(
            math.floor(start_reg / 256), start_reg % 256,
            math.floor(count / 256), count % 256
        )
        local raw = mb_request(0x03, payload)
        local data = check_response(raw, 0x03)
        if not data then return end

        local byte_count = string.byte(data, 1)
        local reg_data = data:sub(2, 1 + byte_count)
        print_registers(start_reg, reg_data, fmt)
    end
}

-- read_ir <start> <count> [fmt]  (FC 04)
H.commands.read_ir = {
    name  = "read_ir",
    usage = "read_ir <start> <count> [hex|signed|float]",
    help  = "Read input registers (FC 04)",
    run   = function(args)
        if not args or args == "" then
            print("usage: read_ir <start_reg> <count> [hex|signed|float]")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end

        local start_reg = tonumber(parts[1]) or 0
        local count     = tonumber(parts[2]) or 1
        local fmt       = parts[3] or "hex"

        if count < 1 or count > 125 then
            print("[!] Count must be 1-125")
            return
        end

        local payload = string.char(
            math.floor(start_reg / 256), start_reg % 256,
            math.floor(count / 256), count % 256
        )
        local raw = mb_request(0x04, payload)
        local data = check_response(raw, 0x04)
        if not data then return end

        local byte_count = string.byte(data, 1)
        local reg_data = data:sub(2, 1 + byte_count)
        print_registers(start_reg, reg_data, fmt)
    end
}

-- write_coil <addr> <ON|OFF|1|0>  (FC 05)
H.commands.write_coil = {
    name  = "write_coil",
    usage = "write_coil <addr> <ON|OFF>",
    help  = "Force single coil (FC 05)",
    run   = function(args)
        if not args or args == "" then
            print("usage: write_coil <addr> <ON|OFF|1|0>")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end

        local addr = tonumber(parts[1]) or 0
        local val_str = (parts[2] or ""):upper()

        local coil_val
        if val_str == "ON" or val_str == "1" then
            coil_val = 0xFF00
        elseif val_str == "OFF" or val_str == "0" then
            coil_val = 0x0000
        else
            print("[!] Value must be ON/OFF or 1/0")
            return
        end

        local payload = string.char(
            math.floor(addr / 256), addr % 256,
            math.floor(coil_val / 256), coil_val % 256
        )
        local raw = mb_request(0x05, payload)
        local data = check_response(raw, 0x05)
        if not data then return end

        printf("Coil %d set to %s\n", addr, coil_val == 0xFF00 and "ON" or "OFF")
    end
}

-- write_sr <reg> <value>  (FC 06)
H.commands.write_sr = {
    name  = "write_sr",
    usage = "write_sr <reg> <value>",
    help  = "Preset single register (FC 06)",
    run   = function(args)
        if not args or args == "" then
            print("usage: write_sr <register> <value>")
            print("  value can be decimal or 0x hex")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end

        local reg = tonumber(parts[1]) or 0
        local val = tonumber(parts[2])
        if not val then
            print("[!] Invalid value")
            return
        end
        val = val % 0x10000

        local payload = string.char(
            math.floor(reg / 256), reg % 256,
            math.floor(val / 256), val % 256
        )
        local raw = mb_request(0x06, payload)
        local data = check_response(raw, 0x06)
        if not data then return end

        printf("Register %d set to 0x%04X (%d)\n", reg, val, val)
    end
}

-- write_mr <start_reg> <val1> [val2] ...  (FC 16)
H.commands.write_mr = {
    name  = "write_mr",
    usage = "write_mr <start_reg> <val1> [val2] ...",
    help  = "Preset multiple registers (FC 16/0x10)",
    run   = function(args)
        if not args or args == "" then
            print("usage: write_mr <start_reg> <value1> [value2] ...")
            return
        end
        local parts = {}
        for w in args:gmatch("%S+") do parts[#parts + 1] = w end

        local start_reg = tonumber(parts[1]) or 0
        local values = {}
        for i = 2, #parts do
            local v = tonumber(parts[i])
            if not v then
                printf("[!] Invalid value: %s\n", parts[i])
                return
            end
            values[#values + 1] = v % 0x10000
        end

        if #values == 0 then
            print("[!] No values specified")
            return
        end

        if #values > 123 then
            print("[!] Max 123 registers per write")
            return
        end

        local count = #values
        local byte_count = count * 2
        local payload_parts = {
            string.char(
                math.floor(start_reg / 256), start_reg % 256,
                math.floor(count / 256), count % 256,
                byte_count
            )
        }
        for _, v in ipairs(values) do
            payload_parts[#payload_parts + 1] = string.char(
                math.floor(v / 256), v % 256
            )
        end
        local payload = table.concat(payload_parts)

        local raw = mb_request(0x10, payload)
        local data = check_response(raw, 0x10)
        if not data then return end

        printf("Wrote %d register(s) starting at %d\n", count, start_reg)
    end
}

-- loopback <data>  (FC 08, sub-function 0x0000)
H.commands.loopback = {
    name  = "loopback",
    usage = "loopback <value>",
    help  = "Diagnostic loopback test (FC 08)",
    run   = function(args)
        if not args or args == "" then
            print("usage: loopback <16-bit value>")
            return
        end
        local val = tonumber(args) or 0
        val = val % 0x10000

        local payload = string.char(
            0x00, 0x00,  -- sub-function 0x0000 = loopback
            math.floor(val / 256), val % 256
        )
        local raw = mb_request(0x08, payload)
        local data = check_response(raw, 0x08)
        if not data then return end

        if #data >= 4 then
            local echo_val = u16(data, 3)
            if echo_val == val then
                printf("Loopback OK: 0x%04X\n", echo_val)
            else
                printf("Loopback MISMATCH: sent 0x%04X, got 0x%04X\n", val, echo_val)
            end
        end
    end
}

-- device_id  (FC 17 / 0x11)
H.commands.device_id = {
    name  = "device_id",
    usage = "device_id",
    help  = "Report device ID (FC 17/0x11)",
    run   = function(_)
        local raw = mb_request(0x11, nil)
        local data = check_response(raw, 0x11)
        if not data then return end

        if #data >= 1 then
            local byte_count = string.byte(data, 1)
            printf("Device ID response (%d bytes):\n", byte_count)
            if #data > 1 then
                printf("  Slave ID   : %d\n", string.byte(data, 2))
            end
            if #data > 2 then
                local run_status = string.byte(data, 3)
                printf("  Run status : 0x%02X (%s)\n", run_status,
                       run_status == 0xFF and "ON" or "OFF")
            end
            if #data > 3 then
                printf("  Extra data : %s\n", sitool.utils.btoh(data:sub(4)))
            end
        end
    end
}

-- scan [start] [end]
H.commands.scan = {
    name  = "scan",
    usage = "scan [start_addr] [end_addr]",
    help  = "Scan for Modbus slaves (FC 17 probe)",
    run   = function(args)
        local start_addr = 1
        local end_addr   = 32

        if args and args ~= "" then
            local parts = {}
            for w in args:gmatch("%S+") do parts[#parts + 1] = w end
            start_addr = tonumber(parts[1]) or 1
            end_addr   = tonumber(parts[2]) or start_addr
        end

        printf("[*] Scanning addresses %d..%d\n", start_addr, end_addr)
        local saved_addr = _slave_addr
        local found = 0

        for addr = start_addr, end_addr do
            _slave_addr = addr

            -- try FC 17 (Report Device ID) as probe
            local raw = mb_request(0x11, nil)
            if raw and #raw >= 3 and check_crc(raw) then
                local resp_addr = string.byte(raw, 1)
                local func = string.byte(raw, 2)
                if resp_addr == addr and func < 0x80 then
                    printf("  [%3d] FOUND (FC 0x%02X responded)\n", addr, func)
                    found = found + 1
                elseif resp_addr == addr then
                    local exc = string.byte(raw, 3)
                    printf("  [%3d] FOUND (exception 0x%02X)\n", addr, exc)
                    found = found + 1
                end
            end
        end

        _slave_addr = saved_addr
        printf("[*] Scan complete: %d device(s) found\n", found)
    end
}

local FC_NAMES = {
    [0x01] = "Read Coils",
    [0x02] = "Read Discrete Inputs",
    [0x03] = "Read Holding Registers",
    [0x04] = "Read Input Registers",
    [0x05] = "Force Single Coil",
    [0x06] = "Preset Single Register",
    [0x08] = "Loopback Diagnostic",
    [0x10] = "Preset Multiple Registers",
    [0x11] = "Report Device ID",
}

H.callbacks.on_recv = function(raw)
    if #raw < 3 then return end

    if not check_crc(raw) then
        printf("[modbus:recv] CRC FAIL len=%d data=%s\n",
               #raw, sitool.utils.btoh(raw))
        return
    end

    local addr = string.byte(raw, 1)
    local func = string.byte(raw, 2)
    local is_exc = func >= 0x80
    local real_func = is_exc and (func - 0x80) or func
    local fc_name = FC_NAMES[real_func] or string.format("FC 0x%02X", real_func)

    if is_exc then
        local exc_code = string.byte(raw, 3)
        local exc_name = EXCEPTION_NAMES[exc_code] or "UNKNOWN"
        printf("[modbus:recv] ADDR=%d EXCEPTION %s (0x%02X) for %s\n",
               addr, exc_name, exc_code, fc_name)
    else
        local data_hex = ""
        if #raw > 4 then
            data_hex = sitool.utils.btoh(raw:sub(3, #raw - 2))
        end
        printf("[modbus:recv] ADDR=%d %s data=[%s]\n",
               addr, fc_name, data_hex)
    end
end

H.callbacks.on_load = function()
    print("[modbus] Modbus RTU master handler loaded")
    printf("[modbus] Default slave address: %d\n", _slave_addr)
    print("[modbus] Recommended settings: 9600 8E1 or 19200 8E1")
    print("[modbus] Type \"help\" for available commands")
end

H.callbacks.on_unload = function()
    print("[modbus] Handler unloaded")
end

return H


     ___  
    | _ )  
    | _ \  
    |___/eemo's  
    . . . . . . sitool  
    . . . . . . Serial Interface Toolkit v2.2.052026  
    . . . . . . nflatrea@mailo.com <Noë Flatreaud>

    DESCRIPTION
    :::::::::::

    sitool is a set of tools to interact with serial ports
    in a fine grained way. You can send raw packets in several
    hex formats or use LUA handlers to script you own set of
    commands and dissectors

    USAGE
    :::::

    Serial Interface Toolkit v2.2.052026

    Usage: ./sitool [-p PORT] [-b BAUDRATE] [-H HANDLER] [-h] [CMD ...]

      -p, --port      PORT       serial port  (e.g. /dev/ttyUSB0)
      -b, --baudrate  BAUDRATE   baud rate    (e.g. 9600, 115200)
      -H, --handler   HANDLER    load Lua handler on start
      -h, --help                 display this help

    If CMD is given, execute it and exit.
    Otherwise enter interactive mode.


    INTERACTIVE MODE
    ::::::::::::::::

    sitool> help

      help       Display help
      exit       Exit sitool
      open       Open serial connection (ex. open /ttyUSB0)
      close      Close serial connection
      set        Set attribute  (set key value)
      get        Get attribute  (get key | get all)
      raw        Send raw payload (raw AA BB "TXT" ...)
      use        Load handler (use <name> | none)
      list       List resources (list handlers | list devices)
      term       Raw terminal (Ctrl-] to quit)

      Attributes: baudrate, databits, parity, stopbits, port, echo

    sitool>


    TERM MODE
    :::::::::

    Type 'term' when connected to enter a raw terminal mode,
    similar to picocom/minicom/screen. Stdin goes directly to
    the serial port, serial output goes to stdout.

    Press Ctrl-] (0x1D) to quit back to the sitool prompt.

    Local echo can be toggled with 'set echo on' / 'set echo off'.

      $ ./sitool -p /dev/ttyUSB0 -b 115200
      ttyUSB0> set echo on
      ttyUSB0> term
      --- term (/dev/ttyUSB0 @ 115200 8N1) | echo on | Ctrl-] to quit ---
      hello
      OK
      ^]
      --- term closed ---
      ttyUSB0>


    LUA HANDLER API
    ::::::::::::::::

    Handlers are Lua scripts that return a table with commands
    and callbacks. The following API is available:

      sitool.send(payload)             Send + receive (sync)
      sitool.write(payload)            Send without waiting for response
      sitool.read()                    Blocking read (VTIME timeout)
      sitool.poll([timeout_ms])        Non-blocking read with timeout
      printf(fmt, ...)                 Formatted print (like C printf)

      sitool.utils.btoh(raw)           Bytes to hex string
      sitool.utils.htob(hex)           Hex string to bytes
      sitool.utils.atob(ascii)         ASCII to bytes
      sitool.utils.btoa(raw)           Bytes to printable ASCII
      sitool.utils.hex(raw)            Hexdump

    Payloads can be hex ("AA BB CC"), ASCII ("\"HELLO\""),
    or mixed ("02 10 \"HELLO\" FF").


    LICENSE
    :::::::

    This repository is licensed under the MIT License. See the LICENSE file for details.

    CONTRIBUTING
    ::::::::::::

    Feel free to contribute by submitting a pull request or opening an issue ;)

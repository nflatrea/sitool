 ___	                                      
| _ )  	                                      
| _ \ 	                                      
|___/eemo's                                   
. . . . . . sitool                            
. . . . . . Serial Interface Toolkit v2.0.052026       
. . . . . . nflatrea@mailo.com <Noë Flatreaud>

DESCRIPTION
:::::::::::

sitool is a set of tools to interact with serial ports
in a fine grained way. You can send raw packets in several
hex formats or use LUA handlers to script you own set of 
commands and dissectors

USAGE
:::::

Serial Interface Toolkit v2.0.052026

Usage: ./sitool [-p PORT] [-b BAUDRATE] [-u HANDLER] [-h] [CMD ...]

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
  get        Get attribute  (get key)
  raw        Send raw payload (raw AA BB "TXT" ...)
  use        Load handler (use <name> | list | none)

  Attributes: baudrate, databits, parity, stopbits, port

sitool> 

LICENSE
:::::::

This repository is licensed under the MIT License. See the LICENSE file for details.

CONTRIBUTING
::::::::::::

Feel free to contribute by submitting a pull request or opening an issue ;)

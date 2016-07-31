reading temperature using a DS18B20 Digital temperature sensor

example program for DS18B20:
http://bildr.org/2011/07/ds18b20-arduino/

# the following instructions are ONLY necessary if you don't want to use the ordinary Arduino IDE to flash the program onto your arduino!!!!!!!!!!!!

# installing the toolchain

First install the standard avr toolchain:

```
$ sudo apt-get install python-pip gcc-avr avr-libc binutils-avr avrdude
```

# build and flash

Plug in the arduino over usb (and make sure you have permission to write to
/dev/ttyACM0), then do:

```
$ ./make.sh
```

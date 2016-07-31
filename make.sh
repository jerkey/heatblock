#!/bin/bash

# stats for the uno from http://arduino.cc/en/uploads/Main/boards.txt
MCU=atmega328p
F_CPU=16000000

if test -n "$1"; then
  PORT=$1
elif test -c /dev/ttyACM0; then
  PORT=/dev/ttyACM0
fi

ROOT=$(cd `dirname $0`; pwd)
mkdir -p $ROOT/build
rm -f $ROOT/build/*

ARDUINO_ROOT=/usr/share/arduino
ARDUINO_PATH=$ARDUINO_ROOT/hardware/arduino/cores/arduino
VARIANTS_PATH=$ARDUINO_ROOT/hardware/arduino/variants/standard

CORE_SOURCES=`echo $ARDUINO_PATH/{main.cpp,wiring*.c,WInterrupts.c,\
{WMath,WString,Print,HardwareSerial}.cpp}`

avr-c++ -o $ROOT/build/main.elf -x c++ \
  $ROOT/hottube.ino \
  <(echo 'extern "C" void __cxa_pure_virtual() { while(1); }') \
  $CORE_SOURCES \
  $PWD/vendor/OneWire/OneWire.cpp \
  $ARDUINO_ROOT/libraries/SPI/SPI.cpp \
  -D F_CPU=$F_CPU -D ARDUINO=110 -mmcu=$MCU \
  -I $PWD/vendor/OneWire \
  -I $ARDUINO_PATH -I $VARIANTS_PATH \
  `find $ARDUINO_ROOT/libraries -type d | sed 's/^/-I /'` \
  -ffunction-sections -fdata-sections -w \
&& avr-objcopy -O ihex -R .eeprom $ROOT/build/main.elf $ROOT/build/main.hex \
&& avrdude -p$MCU -P$PORT -carduino -b115200 -U flash:w:$ROOT/build/main.hex:i

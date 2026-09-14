# Build the stopwatch firmware with avr-gcc.
#
#   make          compile and link, then print the flash/RAM usage
#   make flash    upload to a real Arduino Uno on PORT (default /dev/ttyUSB0)
#   make clean    delete the build output
#
# On Ubuntu/WSL:  sudo apt install gcc-avr avr-libc avrdude

MCU     = atmega328p
F_CPU   = 16000000UL
TARGET  = stopwatch
SRC     = src/stopwatch.c src/main.c
PORT   ?= /dev/ttyUSB0

CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu99 -Wall -Wextra \
          -ffunction-sections -fdata-sections -Isrc
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

all: build/$(TARGET).hex size

build:
	mkdir -p build

build/$(TARGET).elf: $(SRC) | build
	avr-gcc $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

build/$(TARGET).hex: build/$(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

size: build/$(TARGET).elf
	avr-size --format=avr --mcu=$(MCU) $<

flash: build/$(TARGET).hex
	avrdude -c arduino -p $(MCU) -P $(PORT) -b 115200 -U flash:w:$<:i

clean:
	rm -rf build

.PHONY: all size flash clean

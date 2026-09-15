# Bare-Metal Digital Stopwatch (ATmega328P)

A four-digit stopwatch written in C for the ATmega328P **without a single Arduino
library call**. Every pin, every timer and every interrupt is configured by writing
directly to the microcontroller's registers.

**[Run it in your browser](https://wokwi.com/projects/475218749247186945)** on Wokwi, no hardware and no install. The same firmware runs on a real Arduino Uno.

| | |
|---|---|
| Language | C (C99), `avr-gcc` |
| Target | ATmega328P at 16 MHz |
| Flash used | 1086 bytes (3.3 % of 32 kB) |
| RAM used | 31 bytes (1.5 % of 2 kB) |
| Dependencies | none |

## What it does

- Counts in hundredths of a second on a multiplexed 4-digit display
- Shows `SS.cc` below one minute and switches to `MM:SS` above it
- Green button starts and stops; red button resets when stopped and captures
  a lap time when running (the display freezes, the count keeps going)
- Buttons are debounced in firmware, so a single press is registered once

## Why it is written this way

Most beginner projects call `digitalWrite()` and `delay()`. That hides the
microcontroller. This project does the opposite and demonstrates the parts of
the Microprocessors, Microcontrollers and Peripherals syllabus that matter in
industry:

**Timer1 as the time base.** The system clock is 16 MHz. With a prescaler of 64
the timer counts at 250 kHz, so 2500 counts is exactly 10 ms. The timer is put
in CTC mode and `OCR1A` is loaded with 2499, because the counter starts at zero.
The compare-match interrupt then fires exactly 100 times per second. The timing
does not drift, because it comes from the crystal, not from a software loop.

**Timer0 for display refresh.** A 4-digit display has only one set of segment
lines, so the digits are lit one at a time, fast enough that the eye sees all
four. Timer0 overflows at 16 MHz / 64 / 256 = 976 Hz, which gives each digit
244 refreshes per second. Anything above roughly 60 Hz per digit looks steady.

**Debouncing without `delay()`.** A mechanical switch bounces for a few
milliseconds. The same interrupt that refreshes the display samples the buttons
every 8 ms and only accepts a new level after four identical readings, so a
press has to be stable for about 32 ms before it counts. Blocking the CPU with
a delay would stop the clock, so it is never used.

**Sharing data with an interrupt.** The elapsed time is written by the
interrupt and read by the main loop. It is declared `volatile` so the compiler
does not cache it in a register, and the 32-bit read in the main loop is wrapped
in `cli()` / `sei()` so an interrupt cannot change the value halfway through the
read. This is the classic embedded bug that this code deliberately avoids.

## Pin map

| Signal | AVR port | Arduino Uno pin |
|---|---|---|
| Segments a, b, c, d, e, f, g, dp | PD0 - PD7 | D0 - D7 |
| Digit 1 - 4 common cathodes | PB0 - PB3 | D8 - D11 |
| START / STOP button (to GND) | PC0 | A0 |
| RESET / LAP button (to GND) | PC1 | A1 |

Both buttons use the AVR's internal pull-up resistors, so no external resistors
are needed. PD0 and PD1 are also the UART pins, so the serial monitor cannot be
used while the display is running.

On real hardware, put a 220 Ω resistor in each of the eight segment lines. The
simulator does not need them, a real display does.

## Run it in the simulator

1. Open [wokwi.com](https://wokwi.com) and start a new **Arduino Uno** project.
2. Replace the contents of `sketch.ino` with [`wokwi/sketch.ino`](wokwi/sketch.ino).
3. Add two more files with the **+** button next to the file tabs, and paste in
   [`src/stopwatch.c`](src/stopwatch.c) and [`src/stopwatch.h`](src/stopwatch.h).
4. Open the `diagram.json` tab and paste in [`diagram.json`](diagram.json).
5. Press the green play button.

If Wokwi reports an unknown pin name in the diagram, delete the connection it
complains about and drag the wire yourself using the pin map table above. The
table, not the JSON file, is the definitive wiring.

## Build it for real hardware

```bash
sudo apt install gcc-avr avr-libc avrdude   # once
make                                        # compile and report size
make flash PORT=/dev/ttyUSB0                # upload to an Arduino Uno
```

## File layout

```
src/stopwatch.h   public interface: stopwatch_init(), stopwatch_poll()
src/stopwatch.c   all the firmware: timers, interrupts, display, debouncing
src/main.c        entry point for the avr-gcc build
wokwi/sketch.ino  entry point for the Arduino/Wokwi build
diagram.json      the simulator circuit
Makefile          build, size report and flash targets
```

The firmware logic lives in one place and the two entry points just call into
it, so the Arduino build and the bare-metal build can never drift apart.

## Possible extensions

- Store the fastest lap in EEPROM so it survives a power cycle
- Replace the polling main loop with a sleep instruction and wake on interrupt,
  and measure the drop in supply current
- Drive the display through a 74HC595 shift register to free up eight pins

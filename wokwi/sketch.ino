/*
 * sketch.ino - Wokwi / Arduino IDE entry point.
 *
 * The Arduino core supplies its own main(), which calls setup() once and then
 * loop() forever. So the two functions below do exactly what src/main.c does.
 * No Arduino library function is used anywhere in this project: every pin and
 * timer is configured by writing to the ATmega328P registers directly.
 *
 * To run this in Wokwi, add stopwatch.c and stopwatch.h to the sketch as
 * extra files (the "+" button next to the file tabs).
 */
#include "stopwatch.h"

void setup()
{
    stopwatch_init();
}

void loop()
{
    stopwatch_poll();
}

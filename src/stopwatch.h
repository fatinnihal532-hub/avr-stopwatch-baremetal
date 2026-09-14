/*
 * stopwatch.h - public interface of the bare-metal stopwatch firmware.
 *
 * Keeping the entry points in a header means the same firmware can be
 * built two ways without copying any code:
 *   - src/main.c      plain avr-gcc build, main() calls these directly
 *   - wokwi/sketch.ino  Arduino build, setup()/loop() call these instead
 */
#ifndef STOPWATCH_H
#define STOPWATCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the I/O pins and timers, then enable interrupts. Call once. */
void stopwatch_init(void);

/* Handle button events and refresh the display. Call continuously. */
void stopwatch_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* STOPWATCH_H */

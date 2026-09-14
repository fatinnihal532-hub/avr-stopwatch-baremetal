/*
 * main.c - entry point for the plain avr-gcc build.
 *
 * On a microcontroller there is no operating system to return to, so main()
 * never exits. All the work happens in stopwatch.c.
 */
#include "stopwatch.h"

int main(void)
{
    stopwatch_init();

    for (;;) {
        stopwatch_poll();
    }
}

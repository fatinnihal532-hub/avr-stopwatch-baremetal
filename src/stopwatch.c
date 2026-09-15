/*
 * Bare-metal digital stopwatch  -  ATmega328P (Arduino Uno board)
 * ---------------------------------------------------------------
 * No Arduino library functions are used. Every peripheral is set up by
 * writing directly to the microcontroller's registers, the way it is done
 * in the Microprocessors, Microcontrollers and Peripherals course.
 *
 * Peripherals exercised
 *   Timer1  CTC mode, compare-match interrupt  -> 10 ms time base
 *   Timer2  overflow interrupt                 -> display refresh + button scan
 *   PORTD   8 output pins                      -> 7-segment segment lines
 *   PORTB   4 output pins                      -> digit select lines
 *   PORTC   2 input pins with internal pull-ups -> push buttons
 *
 * Hardware map (4-digit common-cathode display)
 *   PD0..PD7 -> segments a b c d e f g dp   (HIGH turns a segment on)
 *   PB0..PB3 -> digit commons               (LOW  turns a digit on)
 *   PC0      -> START / STOP button to GND
 *   PC1      -> RESET / LAP  button to GND
 *
 * Note: PD0 and PD1 are also the UART pins, so the serial monitor cannot be
 * used at the same time as the display. That is a deliberate trade-off and a
 * good thing to be able to explain in an interview.
 *
 * Display format
 *   under 60 s : SS.cc   (seconds and hundredths, decimal point on digit 2)
 *   60 s and up: MM:SS   (minutes and seconds, decimal point used as a colon)
 */

#include "stopwatch.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* ------------------------------------------------------------------ */
/* Hardware definitions                                               */
/* ------------------------------------------------------------------ */
#define SEG_PORT   PORTD
#define SEG_DDR    DDRD
#define DIG_PORT   PORTB
#define DIG_DDR    DDRB
#define BTN_PIN    PINC
#define BTN_PORT   PORTC
#define BTN_DDR    DDRC

#define BTN_START  0          /* bit position of PC0 */
#define BTN_RESET  1          /* bit position of PC1 */
#define DIGIT_MASK 0x0F       /* PB0..PB3 */

/* Segment patterns for 0-9. Bit 0 = segment a ... bit 6 = segment g. */
static const uint8_t FONT[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
#define SEG_DP 0x80

/* ------------------------------------------------------------------ */
/* State shared between the interrupt handlers and main()              */
/* Anything written by an ISR and read by main() must be volatile.     */
/* ------------------------------------------------------------------ */
static volatile uint32_t centiseconds = 0;   /* elapsed time, 1/100 s   */
static volatile uint8_t  running      = 0;   /* 1 while counting        */
static volatile uint8_t  frame        = 0;   /* set 100x/s for main()   */
static volatile uint8_t  events       = 0;   /* button press flags      */

#define EV_START 0x01
#define EV_RESET 0x02

/* Frame buffer the multiplex ISR reads: one segment pattern per digit. */
static volatile uint8_t framebuf[4] = {0, 0, 0, 0};

/* ------------------------------------------------------------------ */
/* Timer1: 10 ms time base                                            */
/* 16 MHz / 64 prescaler = 250 kHz, so 2500 ticks = 10 ms.            */
/* OCR1A is loaded with 2499 because the counter starts at 0.         */
/* ------------------------------------------------------------------ */
static void timer1_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  /* CTC, /64 */
    OCR1A  = 2499;
    TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect)
{
    if (running) {
        centiseconds++;
        if (centiseconds >= 360000UL)   /* wrap after 1 hour */
            centiseconds = 0;
    }
    frame = 1;                          /* tell main() to redraw */
}

/* ------------------------------------------------------------------ */
/* Timer2: display multiplexing and button sampling                   */
/* 16 MHz / 64 / 256 = 976 Hz  ->  244 Hz per digit, flicker free.    */
/* ------------------------------------------------------------------ */
static void timer2_init(void)
{
    TCCR2A = 0;
    TCCR2B = (1 << CS22);                /* normal mode, /64 */
    TIMSK2 = (1 << TOIE2);
}

/* Debounce by requiring the same reading DEBOUNCE_N times in a row.
 * The ISR runs every ~1 ms and we sample every 8th call, so 4 stable
 * samples means the contact has been quiet for about 32 ms. */
#define DEBOUNCE_N 4

ISR(TIMER2_OVF_vect)
{
    static uint8_t digit = 0;
    static uint8_t prescale = 0;
    static uint8_t stable = 0;       /* debounced button levels */
    static uint8_t candidate = 0;
    static uint8_t count = 0;

    /* ---- multiplex one digit ---- */
    DIG_PORT |= DIGIT_MASK;                 /* all digits off first    */
    SEG_PORT  = framebuf[digit];            /* put the pattern out     */
    DIG_PORT &= (uint8_t)~(1 << digit);     /* enable this digit only  */
    digit = (uint8_t)((digit + 1) & 0x03);

    /* ---- sample the buttons every 8th interrupt ---- */
    if (++prescale < 8)
        return;
    prescale = 0;

    /* Buttons pull the pin LOW, so invert to get 1 = pressed. */
    uint8_t raw = (uint8_t)(~BTN_PIN) & ((1 << BTN_START) | (1 << BTN_RESET));

    if (raw != candidate) {
        candidate = raw;
        count = 0;
    } else if (count < DEBOUNCE_N) {
        if (++count == DEBOUNCE_N) {
            uint8_t pressed = (uint8_t)(candidate & ~stable);  /* rising edges */
            if (pressed & (1 << BTN_START)) events |= EV_START;
            if (pressed & (1 << BTN_RESET)) events |= EV_RESET;
            stable = candidate;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Turn an elapsed time into four segment patterns                    */
/* ------------------------------------------------------------------ */
static void render(uint32_t cs)
{
    uint8_t d[4];
    uint8_t dp_index;

    if (cs < 6000UL) {                 /* SS.cc */
        uint8_t sec = (uint8_t)(cs / 100UL);
        uint8_t hun = (uint8_t)(cs % 100UL);
        d[0] = (uint8_t)(sec / 10);
        d[1] = (uint8_t)(sec % 10);
        d[2] = (uint8_t)(hun / 10);
        d[3] = (uint8_t)(hun % 10);
        dp_index = 1;
    } else {                           /* MM:SS */
        uint16_t total = (uint16_t)(cs / 100UL);
        uint8_t min = (uint8_t)(total / 60);
        uint8_t sec = (uint8_t)(total % 60);
        d[0] = (uint8_t)(min / 10);
        d[1] = (uint8_t)(min % 10);
        d[2] = (uint8_t)(sec / 10);
        d[3] = (uint8_t)(sec % 10);
        dp_index = 1;
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pattern = FONT[d[i]];
        if (i == dp_index)
            pattern |= SEG_DP;
        framebuf[i] = pattern;
    }
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                */
/* ------------------------------------------------------------------ */
static uint32_t shown    = 0;   /* value currently on the display */
static uint8_t  lap_held = 0;   /* 1 while a lap time is frozen   */

void stopwatch_init(void)
{
    SEG_DDR  = 0xFF;                     /* segments are outputs   */
    DIG_DDR |= DIGIT_MASK;               /* digit selects outputs  */
    DIG_PORT |= DIGIT_MASK;              /* all digits off         */

    BTN_DDR  &= (uint8_t)~((1 << BTN_START) | (1 << BTN_RESET));   /* inputs */
    BTN_PORT |=  (1 << BTN_START) | (1 << BTN_RESET);              /* pull-ups */

    timer1_init();
    timer2_init();
    sei();                               /* global interrupt enable */

    render(0);
}

void stopwatch_poll(void)
{
    /* --- handle button events --- */
    if (events) {
        uint8_t e;
        cli();                       /* read and clear atomically */
        e = events;
        events = 0;
        sei();

        if (e & EV_START) {
            running = !running;
            lap_held = 0;            /* starting or stopping clears a lap */
        }
        if (e & EV_RESET) {
            if (running) {
                lap_held = !lap_held;     /* freeze or release the display */
            } else {
                cli();
                centiseconds = 0;
                sei();
                lap_held = 0;
            }
        }
    }

    /* --- redraw 100 times a second --- */
    if (frame) {
        frame = 0;
        if (!lap_held) {
            uint32_t now;
            cli();
            now = centiseconds;
            sei();
            if (now != shown) {
                shown = now;
                render(shown);
            }
        }
    }
}

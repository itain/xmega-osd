// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
 * Logo.xbm contains a 128x64 pixel B/W image atored as single array of 1024 bytes, LSB first.
 * Created with The Gimp, saved as XBM file.
 * static unsigned char Logo_bits[1024];
 */

#ifdef MAIN
#define static /*empty*/
#include "Logo192X144.xbm"
#undef static
#else
#define Logo192X144_width 208
#define Logo192X144_height 136
extern unsigned char Logo192X144_bits[];
#endif
#define Logo_bits Logo192X144_bits
#define Logo_width Logo192X144_width
#define Logo_height Logo192X144_height

//#define XTAL16		// use 16 MHz crystal
void Config32MHzClock(void);
void SpektrumInit(void);
void SpektrumUpdateImage(void);
void UpdateClock(void);
void put_char_at(uint8_t ch, uint8_t x, uint8_t y);
uint32_t micros(void);
void FillRectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val);
void read_analog(void);

extern unsigned char Logo_bits[];

#define TIMER_PRESCALER 64
// #define TICKS(n)	((uint16_t)(((uint32_t)(n)*((F_CPU/1000000)))/(TIMER_PRESCALER)))
#define TICKS(n)	((n)/2)		// Microseconds to timer 'ticks' converter

#define DATASIZE ((Logo_width)/8)

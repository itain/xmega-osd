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
#include "Logo208x128.xbm"
#undef static
#else
#define Logo208x128_width 208
#define Logo208x128_height 136
extern unsigned char Logo208x128_bits[];
#endif
#define Logo_bits Logo208x128_bits
#define Logo_width Logo208x128_width
#define Logo_height Logo208x128_height

//#define XTAL16		// use 16 MHz crystal
void Config32MHzClock(void);
void SpektrumInit(void);
void SpektrumUpdateImage(void);
void UpdateClock(void);
void put_char_at(uint8_t ch, uint8_t x, uint8_t y);
uint32_t micros(void);
void FillRectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val);
void DrawLine(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val);
void calibrate_ADC(void);
void read_ADC(void);

extern unsigned char Logo_bits[];

#define TIMER_PRESCALER 64
// #define TICKS(n)	((uint16_t)(((uint32_t)(n)*((F_CPU/1000000)))/(TIMER_PRESCALER)))
#define TICKS(n)	((n)/2)		// Microseconds to timer 'ticks' converter

#define DATASIZE ((Logo_width)/8)

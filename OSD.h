// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define Screen_width 208
#define Screen_height 128
uint8_t Screen_bits[Screen_width*Screen_height/8];

//#define XTAL16		// use 16 MHz crystal
void Config32MHzClock(void);
void SpektrumInit(void);
void SpektrumUpdateImage(void);
void UpdateClock(void);
void put_char_at(char ch, uint8_t x, uint8_t y);
void put_string_at_P(const char *s, uint8_t x, uint8_t y);
uint32_t micros(void);
void FillRectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val);
void DrawLine(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val);
void calibrate_ADC(void);
void read_ADC(void);

#define TIMER_PRESCALER 64
// #define TICKS(n)	((uint16_t)(((uint32_t)(n)*((F_CPU/1000000)))/(TIMER_PRESCALER)))
#define TICKS(n)	((n)/2)		// Microseconds to timer 'ticks' converter

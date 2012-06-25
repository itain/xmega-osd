#include "OSD.h"

#include <stddef.h>
#include <avr/pgmspace.h>
#include <math.h>

static uint8_t ReadCalibrationByte( uint8_t index )
{
	uint8_t result;

	/* Load the NVM Command register to read the calibration row. */
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
	result = pgm_read_byte(index);

	/* Clean up NVM Command register. */
	NVM_CMD = NVM_CMD_NO_OPERATION_gc;

	return result;
}


void calibrate_ADC(void) {
	ADCA.CALL = ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0) );
	ADCA.CALH = ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1) );
}

void read_ADC(void) {
	PORTA.DIR = 0;						// configure PORTA as input
	ADCA.CTRLA |= ADC_ENABLE_bm;				// enable adc
	ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;			// 12 bit conversion
	ADCA.REFCTRL = ADC_REFSEL_VCC_gc /*| ADC_BANDGAP_bm*/;	// internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV128_gc;		// peripheral clk/128
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;	// single ended
	ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc;		// PORTA:2

	ADCA.CH0.CTRL |= ADC_CH_START_bm;			// start conversion on channel 0

	while(!ADCA.CH0.INTFLAGS)
		;

	uint16_t result = ADCA.CH0RES;

	uint16_t z = result;

	for(uint8_t i = 0; i < 8; i++) {
		uint8_t ch;
		if(i > 1 && z == 0)
			ch = 12;
		else {
			ch = z % 10;
			z /= 10;
		}
		put_char_at(ch, 46-6*i, 25);
	}

#define RADIUS  24
#define CENTERX 104
#define CENTERY 53
#define MIN 220
#define MAX 4095
	if(result < MIN)
		result = MIN;
	if(result > MAX)
		result = MAX;

static int8_t oldx = 0;
static int8_t oldy = 0;

	uint8_t x = (int8_t)(RADIUS * sin(2 * M_PI * (result - MIN) / (MAX-MIN)));
	uint8_t y = (int8_t)(RADIUS * cos(2 * M_PI * (result - MIN) / (MAX-MIN)));

	DrawLine(CENTERX-oldx, CENTERX+oldx, CENTERY-oldy, CENTERY+oldy, 0);
	DrawLine(CENTERX-x, CENTERX+x, CENTERY-y, CENTERY+y, 1);

	oldx = x;
	oldy = y;
}

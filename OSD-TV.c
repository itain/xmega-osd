// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#define MAIN
#include "OSD.h"

#define TIMER	TCD0
#define PORT	PORTD
#define USART	USARTD1
#define DMAX	DMA.CH0

// Hardware used:
// Boston Android EVAL-04 XMega Development Board + 16 MHz crystal (and 2x 10pf capacitors)
//
// Output signals:
// PORTD0 - Used to flash LED
// PORTD1 - Video sync output (connect with 1 KOhm resistor to video)
// PORTD7 - Video signal output (connect with 330 Ohm resistor to video)
// PORTD5 - Video clock
// PORTD4 - Debugoutput (to see interrupt handler timing on a logic analyzer)
uint8_t blank_bits[] = { [0 ... (DATASIZE-1)] 0xff };

/*
 * PAL timing, as described here:
 * http://martin.hinner.info/vga/pal.html
 */
#define NORMAL_LINE_SYNC	TICKS(4)
#define NORMAL_LINE_LEN		TICKS(64)
#define NORMAL_LINE_COUNT	305
#define SYNC_HALFLINE_LEN	TICKS(32)

#define VIDEO_START_IN_LINE	TICKS(16)	// when to start DMA

const uint16_t Field1_sync[] = {
	[ 0 ...  5] TICKS(2),	// Pre-equalizing
	[ 6 ... 10] TICKS(30),	// Vertical sync
	[11 ... 15] TICKS(2),	// Post-equalizing
};
#define FILED1_SYNC_LEN	16

const uint16_t Field2_sync[] = {
	[ 0 ...  4] TICKS(2),	// Pre-equalizing
	[ 5 ...  9] TICKS(30),	// Vertical sync
	[10 ... 13] TICKS(2),	// Post-equalizing
};
#define FILED2_SYNC_LEN	14

int main(void) {
	Config32MHzClock();

	// TCC0 provides the micros() function
	TCC0.PER = 0xffff;		// 65535
	TCC0.CTRLB = TC_WGMODE_NORMAL_gc;
	TCC0.INTCTRLA = TC_OVFINTLVL_HI_gc;
	TCC0.CTRLA = TC_CLKSEL_DIV8_gc;	// clk/8

	PORT.DIR = 0xff & ~_BV(2) & ~_BV(6);
	PORT.PIN1CTRL = PORT_INVEN_bm;
	PORT.PIN7CTRL = PORT_INVEN_bm;
	PORT.PIN6CTRL = PORT_OPC_PULLUP_gc;

	// Initialize Usart as SPI Master
	USART.BAUDCTRLA = 2;			// 3.2 Mbit/sec
	USART.BAUDCTRLB = 0;
	USART.CTRLA = 0;			// No interrupt
	USART.CTRLB = USART_TXEN_bm;		// tx only
	USART.CTRLC = USART_CMODE_MSPI_gc|_BV(2);// SPI, lsb first, rising edge;

	// Initialize DMA
	DMA.CTRL = 0;
	DMA.CTRL = DMA_RESET_bm;		// reset DMA controller
	while (DMA.CTRL & DMA_RESET_bm)		// wait reset complete
		;

	// configure DMA controller
	DMA.CTRL = DMA_ENABLE_bm;
	// channel 0
	// **** TODO: reset dma channels (?)
	DMA.CH0.REPCNT = 1;
	DMA.CH0.CTRLA = DMA_CH_BURSTLEN_1BYTE_gc /*| DMA_CH_SINGLE_bm*/ | DMA_CH_REPEAT_bm;
	DMA.CH0.ADDRCTRL = DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTDIR_FIXED_gc;
	DMA.CH0.DESTADDR0 = (( (uint16_t) &USART.DATA) >> 0) & 0xFF;
	DMA.CH0.DESTADDR1 = (( (uint16_t) &USART.DATA) >> 8) & 0xFF;
	DMA.CH0.DESTADDR2 = 0;
	// Interrupts
	//DMA.CH0.CTRLB = DMA_CH_ERRINTLVL_HI_gc|DMA_CH_TRNINTLVL_HI_gc;
	DMA.CH0.CTRLB = 0;	// No interrupt!

	// Initialize timer
	TIMER.CTRLA = TC_CLKSEL_DIV64_gc;
	TIMER.CTRLB = TC_WGMODE_SS_gc;
	TIMER.PER = NORMAL_LINE_LEN-1;

	TIMER.CCA = VIDEO_START_IN_LINE;
	TIMER.CCB = NORMAL_LINE_SYNC;

	// Interrupt on overflow and CCA
	TIMER.INTCTRLA = TC_OVFINTLVL_HI_gc;
	TIMER.INTCTRLB = TC_CCAINTLVL_HI_gc;

	// Enable timer
	TIMER.CTRLB = TC0_CCBEN_bm|TC_WGMODE_SS_gc;

	// Enable the DMA
//	DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm;

	// Enable interrupts
	PMIC.CTRL = (PMIC_HILVLEN_bm|PMIC_MEDLVLEX_bm|PMIC_LOLVLEX_bm);

	SpektrumInit();

	sei();

	// Now blink LED at 2Hz, use timer TCC0 
	uint32_t new_micros, old_micros250ms = 0, old_micros50ms = 0, old_micros100000ms = 0;
		
	while(1) {
		new_micros = micros();
		if((uint32_t)(new_micros - old_micros250ms) >= 250000) {
			old_micros250ms = new_micros;
			PORTD.OUT ^= _BV(0);
		}
		if((uint32_t)(new_micros - old_micros50ms) >= 5000) {
			old_micros50ms = new_micros;
			SpektrumUpdateImage();
		}
		if((uint32_t)(new_micros - old_micros100000ms) >= 100000) {
			old_micros100000ms = new_micros;
			UpdateClock();
		}
	}
}

uint16_t mode_limit[] = { NORMAL_LINE_COUNT, FILED2_SYNC_LEN, NORMAL_LINE_COUNT, FILED1_SYNC_LEN };
uint16_t count = 0;
uint8_t mode = 0;

ISR(TCD0_OVF_vect) {
	PORT.OUT |= 0x10;	// Debug!

	if((mode & 0x01) == 0) {
		// Normal line, prepare to display some data
		uint8_t *linedata;
		uint8_t effective_line = (count - 29) / 2;

		if(effective_line >= Logo_height)
			linedata = blank_bits;	// below 29 or above 29+4*64
		else
			linedata = &Logo_bits[DATASIZE * effective_line];

		// Prepare timer
		TIMER.PER = NORMAL_LINE_LEN-1;
		TIMER.CCA = VIDEO_START_IN_LINE;
		TIMER.CCB = NORMAL_LINE_SYNC;

		// Prepare DMA to start on CCB;
		DMA.CH0.CTRLB |= DMA_CH_ERRIF_bm;
		DMA.CH0.CTRLB |= DMA_CH_TRNIF_bm;
		DMA.CH0.CTRLA &= DMA_CH_ENABLE_bm;

		//DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_TCD0_CCB_gc;
		DMA.CH0.REPCNT = 1;
		DMA.CH0.CTRLA = DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_SINGLE_bm | DMA_CH_REPEAT_bm;
		//DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_USARTD1_DRE_gc;
		DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_TCD0_CCA_gc;
		DMA.CH0.TRFCNT = DATASIZE;
		DMA.CH0.SRCADDR0 = (( (uint16_t) &linedata[0]) >> 0) & 0xFF;
		DMA.CH0.SRCADDR1 = (( (uint16_t) &linedata[0]) >> 8) & 0xFF;
		DMA.CH0.SRCADDR2 = 0;
		DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm;
	}
	else {
		if(mode & 0x02)
			TIMER.CCB = Field1_sync[count];
		else
			TIMER.CCB = Field2_sync[count];

		TIMER.PER = SYNC_HALFLINE_LEN-1;
		// Large number! (so we don't get the interrupt)
		TIMER.CCA = TICKS(99);
	}

	if(++count >= mode_limit[mode]) {
		count = 0;
		mode = (mode + 1) & 0x03;
	}

	PORT.OUT &= ~0x10;	// Debug!
}

ISR(TCD0_CCA_vect) {
	PORT.OUT |= 0x10;	// Debug!

	// Change trigger of DMA channel
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_USARTD1_DRE_gc;

	PORT.OUT &= ~0x10;	// Debug!
}

volatile uint32_t high_bytes;
ISR(TCC0_OVF_vect)  {
	++high_bytes;
}

uint32_t micros() {
	uint8_t sreg = SREG;
	cli();
	uint32_t res1 = high_bytes;
	uint16_t res2 = TCC0.CNT;
	if(TCC0.INTFLAGS & TC0_OVFIF_bm)
		++res1;
	SREG = sreg;
	return (uint32_t)((res2 >> 2) | (res1 << 14));
}

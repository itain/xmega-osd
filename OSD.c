// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#define MAIN
#include "OSD.h"

//Debug
#ifdef DEBUG
#define ISR_IN	PORTE.OUTSET = _BV(2)
#define ISR_OUT	PORTE.OUTCLR = _BV(2)
#else
#define ISR_IN
#define ISR_OUT
#endif

int main(void) {
	Config32MHzClock();

	// TCC0 provides the micros() function
	TCC0.PER = 0xffff;		// 65535
	TCC0.CTRLB = TC_WGMODE_NORMAL_gc;
	TCC0.INTCTRLA = TC_OVFINTLVL_HI_gc;
	TCC0.CTRLA = TC_CLKSEL_DIV8_gc;	// clk/8

	PORTE.DIR = _BV(3)|_BV(2);
	PORTE.PIN0CTRL = PORT_ISC_FALLING_gc;
	PORTE.PIN1CTRL = PORT_ISC_FALLING_gc;
	PORTE.PIN3CTRL = PORT_INVEN_bm|PORT_OPC_WIREDOR_gc;
	PORTE.INT0MASK = _BV(0);
	PORTE.INT1MASK = _BV(1);
	PORTE.INTCTRL = PORT_INT0LVL_HI_gc|PORT_INT1LVL_HI_gc;

	EVSYS.CH0CTRL = 0;
	EVSYS.CH0MUX = EVSYS_CHMUX_PORTE_PIN0_gc;

	// Initialize Usart as SPI Master
	USARTE0.BAUDCTRLA = 2;			// 3.2 Mbit/sec
	USARTE0.BAUDCTRLB = 0;
	USARTE0.CTRLA = 0;			// No interrupt
	USARTE0.CTRLB = USART_TXEN_bm;		// tx only
	USARTE0.CTRLC = USART_CMODE_MSPI_gc|_BV(2);// SPI, lsb first, rising edge;

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
	DMA.CH0.DESTADDR0 = (( (uint16_t) &USARTE0.DATA) >> 0) & 0xFF;
	DMA.CH0.DESTADDR1 = (( (uint16_t) &USARTE0.DATA) >> 8) & 0xFF;
	DMA.CH0.DESTADDR2 = 0;
	// Interrupts
	//DMA.CH0.CTRLB = DMA_CH_ERRINTLVL_HI_gc|DMA_CH_TRNINTLVL_HI_gc;
	DMA.CH0.CTRLB = 0;	// No interrupt!

	// Initialize timer
	TCE0.CTRLA = TC_CLKSEL_DIV1_gc;
	TCE0.CTRLB = 0;
	TCE0.PER = 0xffff;

	// TCE0.CTRLD = TC_EVACT_CAPT_gc | TC_EVSEL_CH0_gc;
	// TCE0.CTRLB = TC0_CCAEN_bm;

	// Enable timer
	TCE0.CTRLB = TC0_CCBEN_bm|TC_WGMODE_SS_gc;

	// Enable interrupts
	PMIC.CTRL = (PMIC_HILVLEN_bm|PMIC_MEDLVLEX_bm|PMIC_LOLVLEX_bm);

	SpektrumInit();

	sei();

	// Now blink LED at 2Hz, use timer TCC0 
	uint32_t new_micros, old_micros250ms = 0, old_micros50ms = 0, old_micros100000ms = 0;
		
	uint8_t led = 0;
	while(1) {
		new_micros = micros();
		if((uint32_t)(new_micros - old_micros250ms) >= 250000) {
			old_micros250ms = new_micros;
			PORTD.OUTTGL |= _BV(0);
			led = !led;
			FillRectangle(195, 201, 15, 21, led);
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

ISR(TCE0_CCA_vect) {
	ISR_IN;
	// Change trigger of DMA channel
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_DRE_gc;
	ISR_OUT;
}

uint16_t line_ctr;

ISR(PORTE_INT0_vect) {
	ISR_IN;

	while(!(PORTE.IN & _BV(0)))
		;

	TCE0.CCA = TCE0.CNT + 380;
	++line_ctr;
		
	uint8_t *linedata;
	uint8_t effective_line = (line_ctr - 46) / 2;

	if(effective_line >= Logo_height) {
		TCE0.INTCTRLB = 0;
		ISR_OUT;
		return;
	}

	linedata = &Logo_bits[DATASIZE * effective_line];

	// Prepare timer
	TCE0.INTCTRLB = TC_CCAINTLVL_HI_gc;

	// Prepare DMA to start on CCA;
	DMA.CH0.CTRLB |= DMA_CH_ERRIF_bm;
	DMA.CH0.CTRLB |= DMA_CH_TRNIF_bm;
	DMA.CH0.CTRLA &= ~DMA_CH_ENABLE_bm;

	DMA.CH0.REPCNT = 1;
	DMA.CH0.CTRLA = DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_SINGLE_bm | DMA_CH_REPEAT_bm;
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_TCE0_CCA_gc;
	DMA.CH0.TRFCNT = DATASIZE;
	DMA.CH0.SRCADDR0 = (( (uint16_t) &linedata[0]) >> 0) & 0xFF;
	DMA.CH0.SRCADDR1 = (( (uint16_t) &linedata[0]) >> 8) & 0xFF;
	DMA.CH0.SRCADDR2 = 0;
	DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm;
	ISR_OUT;
}

ISR(PORTE_INT1_vect) {
	ISR_IN;
	line_ctr = 0;
	ISR_OUT;
}
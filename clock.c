// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include "OSD.h"

void Config32MHzClock(void)
{
#ifdef XTAL16
	// External 16 MHz crystal
	CCP = CCP_IOREG_gc;
	CLK.LOCK = 0;

	// External 16,000,000kHz oscillator initialization
	OSC.XOSCCTRL = OSC_FRQRANGE_12TO16_gc|OSC_XOSCSEL_XTAL_16KCLK_gc;
	// Enable the oscillator
	OSC.CTRL |= OSC_XOSCEN_bm;

	// Wait for the external oscillator to stabilize
	while (!(OSC.STATUS & OSC_XOSCRDY_bm))
		;

	// Enable PLL and set it to multiply by 2 to get 32MHZ
	OSC.PLLCTRL = OSC_PLLSRC_XOSC_gc|OSC_PLLFAC1_bm;
	OSC.CTRL |= OSC_PLLEN_bm;

 	// Wait for the PLL to stabilize
	while (!(OSC.STATUS & OSC_PLLRDY_bm))
		;

	// Select PLL for the system clock source
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_PLL_gc;

 	// Disable unused clocks
	CCP = CCP_IOREG_gc;
	OSC.CTRL = OSC_XOSCEN_bm|OSC_PLLEN_bm;
#else
	// Internal 32 MHz oscilator
	CCP = CCP_IOREG_gc; //Security Signature to modify clock
	// initialize clock source to be 32MHz internal oscillator (no PLL)
	OSC.CTRL = OSC_RC32MEN_bm; // enable internal 32MHz oscillator
	while(!(OSC.STATUS & OSC_RC32MRDY_bm)); // wait for oscillator ready
	CCP = CCP_IOREG_gc; //Security Signature to modify clock
	CLK.CTRL = 0x01; //select sysclock 32MHz osc
#endif

	// No prescaler
	CCP = CCP_IOREG_gc;
	CLK.PSCTRL = 0;

	CCP = CCP_IOREG_gc;
	CLK.LOCK = 1;
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

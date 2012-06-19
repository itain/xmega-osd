// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include "OSD.h"

#define SPEK_MAX_CHANNEL 8
#define SPEK_FRAME_SIZE 16
#define SPEKTRUM 2048
#if (SPEKTRUM == 1024)
    #define SPEK_CHAN_SHIFT  2       // Assumes 10 bit frames, that is 1024 mode.
    #define SPEK_CHAN_MASK   0x03    // Assumes 10 bit frames, that is 1024 mode.
#endif
#if (SPEKTRUM == 2048)
    #define SPEK_CHAN_SHIFT  3       // Assumes 11 bit frames, that is 2048 mode.
    #define SPEK_CHAN_MASK   0x07    // Assumes 11 bit frames, that is 2048 mode.
#endif


void SpektrumInit(void)
{
	if(!(PORTD.IN & _BV(6))) {	// PIN6  pulled to ground
		// Implement binding protocol for spektrum satellite receiver
		PORTD.DIRSET = _BV(2);
		PORTD.OUT |= _BV(2);

		_delay_ms(73);

		for(int i = 0; i < 5; i++) {
			PORTD.OUT &= ~_BV(2);
			_delay_us(120);
			PORTD.OUT |= _BV(2);
			_delay_us(120);
		}

		_delay_ms(1000);
		PORTD.DIRCLR = _BV(2);
		PORTD.OUT &= ~_BV(2);
	}

	// Init USARTD0 for spektrum satellite.
	// Set USART Format to 8bit, no parity, 1 stop bit
	USARTD0.CTRLC = USART_CHSIZE_8BIT_gc | USART_PMODE_DISABLED_gc;	

	// 115200 @ 32Mhz as calculated from ProtoTalk Calc
	int bsel = 1047;
	uint8_t bscale = 10;

	USARTD0.BAUDCTRLA = (uint8_t) bsel;
	USARTD0.BAUDCTRLB = (bscale << 4) | (bsel >> 8);

	USARTD0.CTRLA = USART_RXCINTLVL_MED_gc;

	// Enable RX only
	USARTD0.CTRLB |= USART_RXEN_bm;
}


volatile uint8_t spekFrame[SPEK_FRAME_SIZE];
volatile uint8_t rcFrameComplete;

ISR(USARTD0_RXC_vect) {
        PORTD.OUT |= 0x10;       // Debug!

	uint32_t spekTime;
	static uint32_t spekTimeLast;
	static uint8_t  spekFramePosition;
	spekTime = micros();
	uint32_t spekTimeInterval = spekTime - spekTimeLast;
	spekTimeLast = spekTime;
	if (spekTimeInterval > 5000)
		spekFramePosition = 0;
	spekFrame[spekFramePosition] = USARTD0.DATA;

	if (spekFramePosition == SPEK_FRAME_SIZE - 1) {
		rcFrameComplete = 1;
	}
	else {
		spekFramePosition++;
	}   

	PORTD.OUT &= ~0x10;      // Debug!
}

uint16_t readRawRC(uint8_t chan) {
	uint16_t data;
	cli();

	static uint16_t spekChannelData[SPEK_MAX_CHANNEL];
	if (rcFrameComplete) {
		for (uint8_t b = 3; b < SPEK_FRAME_SIZE; b += 2) {
			uint8_t spekChannel = 0x0F & (spekFrame[b - 1] >> SPEK_CHAN_SHIFT);
			if (spekChannel < SPEK_MAX_CHANNEL)
				spekChannelData[spekChannel] = ((uint16_t)(spekFrame[b - 1] & SPEK_CHAN_MASK) << 8) + spekFrame[b];
		}
		rcFrameComplete = 0;
	}

	sei();

	if (chan >= SPEK_MAX_CHANNEL) {
		data = 1500;
	}
	else {
#if (SPEKTRUM == 1024)
		data = 988 + spekChannelData[chan];          // 1024 mode
#endif
#if (SPEKTRUM == 2048)
		data = 988 + (spekChannelData[chan] >> 1);   // 2048 mode
#endif
	}

	return data; // We return the value correctly copied when the IRQ's where disabled
}

void SpektrumUpdateImage(void) {
	for(uint8_t ch = 0; ch < SPEK_MAX_CHANNEL; ch++) {
		uint16_t val = readRawRC(ch);

		if(val < 1000)
			val = 0;
		else if(val >= 1000+1024)
			val = 1023;
		else
			val -= 1000;

		val /= 16;

		for(uint8_t j = 0; j < 2; j++) {
			for(uint8_t i = 0; i < 8; i++) {
				uint8_t b = 0;
				if(i > val/8)
					b = 0xff;
				else if(i < val/8)
					b = 0x00;
				else
					b = (uint8_t)~(0xff >> (7-(val & 0x07)));
				//b = spekFrame[2*ch + (1&i)];
				Logo_bits[DATASIZE*(j + 90 + ch * 3) + 1 + i] = b;
			}
		}
	}
}

#ifdef NEW_SPEKTRUM

typedef struct SpektrumStateStruct {
	uint8_t primary;
	uint8_t ReSync;
	uint8_t SpektrumTimer;
	uint8_t Sync;
	uint8_t ChannelCnt;
	uint8_t FrameCnt;
	uint8_t HighByte;
	uint8_t SecondFrame;
	uint16_t LostFrameCnt;
	uint8_t RcAvailable;
	int16_t values[SPEKTRUM_CHANNELS_PER_FRAME*MAX_SPEKTRUM_FRAMES];
} SpektrumStateType;

SpektrumStateType PrimarySpektrumState = {1,0,0,0,0,0,0,0,0};

void SpektrumParser(uint8_t c, SpektrumStateType state) {
	uint16_t ChannelData;
	uint8_t TimedOut;
	static uint8_t TmpEncType = 0;		/* 0 = 10bit, 1 = 11 bit */
	static uint8_t TmpExpFrames = 0;	/* # of frames for channel data */

	TimedOut = (state.SpektrumTimer) ? 1 : 0;

	/* If we have just started the resync process or */
	/* if we have recieved a character before our */
	/* 7ms wait has finished */
	if ((state.ReSync == 1) || ((state.Sync == 0) && (!TimedOut))) {
		state.ReSync = 0;
		state.SpektrumTimer = MIN_FRAME_SPACE;
		state.Sync = 0;
		state.ChannelCnt = 0;
		state.FrameCnt = 0;
		state.SecondFrame = 0;
		return;
	}

	/* the first byte of a new frame. It was received */
	/* more than 7ms after the last received byte. */
	/* It represents the number of lost frames so far.*/
	if (state.Sync == 0) {
		state.LostFrameCnt = _c;
		if(!state.primary) /* secondary receiver */
			state.LostFrameCnt = state.LostFrameCnt << 8;
		state.Sync = 1;
		state.SpektrumTimer = MAX_BYTE_SPACE;
		return;
	}

	/* all other bytes should be recieved within */
	/* MAX_BYTE_SPACE time of the last byte received */
	/* otherwise something went wrong resynchronise */
	if(TimedOut) {
		state.ReSync = 1;
		/* next frame not expected sooner than 7ms */
		state.SpektrumTimer = MIN_FRAME_SPACE;
		return;
	}

	/* second character determines resolution and frame rate for main */
	/* receiver or low byte of LostFrameCount for secondary receiver */
	if(state.Sync == 1) {
		if(_receiver) {
			state.LostFrameCnt +=_c;
			TmpExpFrames = ExpectedFrames;
		}
		else {
			/* TODO: collect more data. I suspect that there is a low res */
			/* protocol that is still 10 bit but without using the full range. */
			TmpEncType =(_c & 0x10)>>4; /* 0 = 10bit, 1 = 11 bit */
			TmpExpFrames = _c & 0x03; /* 1 = 1 frame contains all channels */
			/* 2 = 2 channel data in 2 frames */
		}
		state.Sync = 2;
		state.SpektrumTimer = MAX_BYTE_SPACE;
		return;
	}

	/* high byte of channel data if this is the first byte */
	/* of channel data and the most significant bit is set */
	/* then this is the second frame of channel data. */
	if(state.Sync == 2) {
		state.HighByte = _c;
		if (state.ChannelCnt == 0) {
			state.SecondFrame = (state.HighByte & 0x80) ? 1 : 0;
		}
		state.Sync = 3;
		state.SpektrumTimer = MAX_BYTE_SPACE;
		return;
	}

	/* low byte of channel data */
	if(state.Sync == 3) {
		state.Sync = 2;
		state.SpektrumTimer = MAX_BYTE_SPACE;
		/* we overwrite the buffer now so rc data is not available now */
		state.RcAvailable = 0;
		ChannelData = ((uint16_t)state.HighByte << 8) | _c;
		state.values[state.ChannelCnt + (state.SecondFrame * 7)] = ChannelData;
		state.ChannelCnt ++;
	}

	/* If we have a whole frame */
	if(state.ChannelCnt >= SPEKTRUM_CHANNELS_PER_FRAME) {
		/* how many frames did we expect ? */
		++state.FrameCnt;
		if (state.FrameCnt == TmpExpFrames) {
			/* set the rc_available_flag */
			state.RcAvailable = 1;
			state.FrameCnt = 0;
		}
		if(state.primary) { /* main receiver */
			EncodingType = TmpEncType; /* only update on a good */
			ExpectedFrames = TmpExpFrames; /* main receiver frame */
		}
		state.Sync = 0;
		state.ChannelCnt = 0;
		state.SecondFrame = 0;
		state.SpektrumTimer = MIN_FRAME_SPACE;
	}
}
#endif

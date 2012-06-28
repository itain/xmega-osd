// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include "OSD.h"

#include <avr/pgmspace.h>

#include "Font5x7.xbm"

void put_char_at(uint8_t ch, uint8_t x, uint8_t y) {
	uint16_t index = y * DATASIZE + (x >> 3);
	x &= 0x07;
	ch =  (ch - ' ') << 3;

	for(int i = 0; i < 8; i++) {
		uint8_t b = pgm_read_byte(&Font5x7_bits[ch]);

		uint8_t p = Logo_bits[index];
		p |= (0x3f << x);
		p &= ~(b << x);
		Logo_bits[index] = p;

		if(x > 2) {
			uint8_t xx = 8-x;
			uint8_t p = Logo_bits[index+1];
			p |= (0x3f >> xx);
			p &= ~(b >> xx);
			Logo_bits[index+1] = p;
		}

		++ch;
		index += DATASIZE;
	}
}

uint32_t cur_time;
void UpdateClock(void) {
	uint32_t z = ++cur_time;
	for(uint8_t i = 0; i < 8; i++) {
		uint8_t ch;
		switch(i) {
		default:
			if(i > 5 && z == 0)
				ch = ' ';
			else {
				ch = '0' + z % 10;
				z /= 10;
			}
			break;
		case 1:
			ch = '.';
			break;
		case 3:
			ch = '0' + z % 6;
			z /= 6;
			break;
		case 4:
			ch = ':';
			break;
		}	
		put_char_at(ch, 46-6*i, 4);
	}
}

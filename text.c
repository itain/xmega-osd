// TV Out for xmega
// (C) Itai Nahshon 2012
// nahshon <at> actcom <dot> co <dot> il

#include "OSD.h"

#include <avr/pgmspace.h>

#include "Font5x7.xbm"

void put_char_at(char ch, uint8_t x, uint8_t y) {
	uint16_t index = y * (Screen_width/8) + (x >> 3);
	uint16_t ix =  (ch - ' ') << 3;
	x &= 0x07;

	for(uint8_t i = 0; i < 8; i++) {
		uint8_t b = pgm_read_byte(&Font5x7_bits[ix]);

		uint8_t p = Screen_bits[index];
		p |= (0x3f << x);
		p &= ~(b << x);
		Screen_bits[index] = p;

		if(x > 2) {
			uint8_t xx = 8-x;
			uint8_t p = Screen_bits[index+1];
			p |= (0x3f >> xx);
			p &= ~(b >> xx);
			Screen_bits[index+1] = p;
		}

		++ix;
		index += (Screen_width/8);
	}
}

void put_string_at_P(const char *s, uint8_t x, uint8_t y) {
	for(;;) {
		char ch = *s++;
		if(ch == 0)
			break;
		put_char_at(ch, x, y);
		x += 6;
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

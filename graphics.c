#include "OSD.h"

void FillRectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val) {
	for(uint8_t y = y0; y <= y1; y++) {
		if(y >= Logo_height)
			break;
		for(uint8_t x = x0; x <= x1; x++) {
			if(x >= Logo_width)
				break;
			uint8_t i = x >> 3;
			uint8_t v = (1 << (x & 0x07));
			if(val)
				Logo_bits[y * DATASIZE + i] |= v;
			else
				Logo_bits[y * DATASIZE + i] &= ~v;
		}
	}	
}

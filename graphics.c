#include "OSD.h"

void FillRectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val) {
	for(uint8_t y = y0; y <= y1; y++) {
		if(y >= Screen_height)
			break;
		for(uint8_t x = x0; x <= x1; x++) {
			if(x >= Screen_width)
				break;
			uint8_t i = x >> 3;
			uint8_t v = (1 << (x & 0x07));
			if(val)
				Screen_bits[y * (Screen_width/8) + i] &= ~v;
			else
				Screen_bits[y * (Screen_width/8) + i] |= v;
		}
	}	
}

#define ABS(x) (((x) < 0)?-(x):(x))
void DrawLine(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, uint8_t val) {
	int16_t dx = ABS(x1-x0);
	int16_t dy = ABS(y1-y0);
	int8_t sx = (x0 < x1) ? 1 : -1;
	int8_t sy = (y0 < y1) ? 1 : -1;

	int16_t err = dx - dy;

	for(;;) {
		FillRectangle(x0, x0, y0, y0, val);
		if(x0 == x1 && y0 == y1)
			break;
		int16_t e2 = 2 * err;
		if(e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if(e2 <  dx) {
			err += dx;
			y0 += sy;
		}
	}
}

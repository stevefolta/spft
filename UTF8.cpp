#include "UTF8.h"
#include <string.h>


int UTF8::num_characters(const char* bytes, int length)
{
	// Assumes valid UTF8.
	const char* p = bytes;
	const char* end = bytes + length;
	int num_chars = 0;
	while (p < end) {
		unsigned char c = *p++;
		if (c < 0x80)
			num_chars += 1;
		else if ((c & 0xFD) == 0xFC) {
			num_chars += 6;
			p += 5;
			}
		else if ((c & 0xFC) == 0xF8) {
			num_chars += 5;
			p += 4;
			}
		else if ((c & 0xF8) == 0xF0) {
			num_chars += 4;
			p += 3;
			}
		else if ((c & 0xF0) == 0xE0) {
			num_chars += 3;
			p += 3;
			}
		else if ((c & 0xE0) == 0xC0) {
			num_chars += 2;
			p += 1;
			}
		else {
			// Invalid, but we don't handle that.
			}
		}
	return num_chars;
}




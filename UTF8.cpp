#include "UTF8.h"
#include <string.h>


int UTF8::num_characters(const char* bytes, int length)
{
	// Assumes valid UTF8.
	const unsigned char* p = (const unsigned char*) bytes;
	const unsigned char* end = p + length;
	int num_chars = 0;
	for (; p < end; ++p) {
		unsigned char c = *p;
		if (c < 0x80 || c >= 0xC0)
			num_chars += 1;
		}
	return num_chars;
}


int UTF8::bytes_for_n_characters(const char* bytes, int length, int n)
{
	// Assumes valid UTF8.
	const char* p = bytes;
	const char* end = bytes + length;
	for (; p < end; ++p) {
		unsigned char c = *p;
		if (c < 0x80 || c >= 0xC0) {
			if (n <= 0)
				break;
			n -= 1;
			}
		}
	return p - bytes;
}




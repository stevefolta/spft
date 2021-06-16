#include "History.h"

History::History()
{
	/***/
}


int History::num_lines()
{
	/***/
	return 0;
}


const char* History::line(int which_line)
{
	/***/
	return "";
}


int History::add_input(const char* input, int length)
{
	const char* p = input;
	const char* end = input + length;

	while (p < end) {
		const char* run_start = p;
		switch (*p++) {
			case '\x7F':
				// DEL.
				//***
				break;

			case '\x1B':
				// ESC.
				//***/
				break;

			case '\n':
				//***
				break;

			default:
				// Normal run of characters.
				// TODO: Don't consume partial UTF8 character at end.
				while (p < end) {
					unsigned char c = *p;
					if (c < ' ' || c == '\x7F')
						break;
					++p;
					}
				// Add to current line.
				add_to_current_line(run_start, p);
				break;
			}
		}

	return p - input;
}


void History::add_to_current_line(const char* start, const char* end)
{
	/***/
}






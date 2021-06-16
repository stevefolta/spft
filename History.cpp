#include "History.h"
#include "Line.h"


History::History()
{
	at_end_of_line = true;
	capacity = 10000;
	first_line = last_line = first_line_index = 0;
	current_line = 0;
	lines = new Line*[capacity];
	for (int64_t i = 0; i < capacity; ++i)
		lines[i] = nullptr;
	lines[0] = new Line();
}


History::~History()
{
	for (int64_t i = 0; i < capacity; ++i)
		delete lines[i];
	delete[] lines;
}


int64_t History::num_lines()
{
	return last_line;
}


Line* History::line(int64_t which_line)
{
	return lines[(first_line_index + (which_line - first_line)) % capacity];
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
	Line* cur_line = line(current_line);
	if (at_end_of_line)
		cur_line->append_characters(start, end - start, current_style);
	else {
		//*** TODO
		}
}






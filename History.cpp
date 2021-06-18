#include "History.h"
#include "Line.h"
#include "UTF8.h"


History::History()
{
	at_end_of_line = true;
	capacity = 10000;
	first_line = last_line = first_line_index = 0;
	current_line = 0;
	current_column = 0;
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
	char c;

	while (p < end) {
		const char* run_start = p;
		switch (*p++) {
			case '\x7F':
				// DEL.
				//*** TODO
				break;

			case '\x1B':
				// ESC.
				if (p >= end)
					goto unfinished_run;
				c = *p++;
				if (c >= 0x40 && c <= 0x5F) {
					// "Fe" escape sequence.
					const char* parse_end;
					switch (c) {
						case '[':
							parse_end = parse_csi(p, end);
							if (parse_end == nullptr)
								goto unfinished_run;
							p = parse_end;
							break;
						case 'P':
							parse_end = parse_dcs(p, end);
							if (parse_end == nullptr)
								goto unfinished_run;
							p = parse_end;
							break;
						case ']':
							parse_end = parse_osc(p, end);
							if (parse_end == nullptr)
								goto unfinished_run;
							p = parse_end;
							break;
						case 'X':
						case '^':
						case '_':
							// We ignore these.
							parse_end = parse_st_string(p, end);
							if (parse_end == nullptr)
								goto unfinished_run;
							p = parse_end;
							break;
						default:
							// Unimplemented.
							// We assume only one character follows the ESC.
							break;
						}
					}
				else if (c >= 0x60 && c <= 0x7E) {
					// "Fs" escape sequence.
					// Not currently implemented.  We assume only one character
					// follows the ESC.
					}
				else if (c >= 0x30 && c <= 0x3F) {
					// "Fp" escape sequence ("private use").
					// We assume only one character follows the ESC.
					}
				else if (c >= 0x20 && c <= 0x2F) {
					// "nF" escape sequence.
					// Not currently implemented.
					while (true) {
						if (p >= end)
							goto unfinished_run;
						c = *p++;
						if (c >= 0x30 && c <= 0x7E)
							break;
						else if (c < 0x20 || c > 0x2F) {
							// Not valid.  We'll just terminate the escape sequence there.
							break;
							}
						}
					}
				break;

			case '\n':
				current_column = 0;
				break;

			case '\r':
				if (current_line >= last_line)
					new_line();
				else
					current_line += 1;
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
				current_column += UTF8::num_characters(run_start, p - run_start);
				break;

			unfinished_run:
				return run_start - input;
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


void History::new_line()
{
	last_line += 1;
	current_line = last_line;
	if (last_line >= capacity) {
		// History is full, we'll recycle the previous first line.
		lines[first_line_index]->clear();
		first_line += 1;
		first_line_index += 1;
		if (first_line_index >= capacity)
			first_line_index = 0;
		}
	else {
		// We may not have allocated the Line yet.
		if (lines[last_line] == nullptr)
			lines[last_line] = new Line();
		}
}


const char* History::parse_csi(const char* p, const char* end)
{
	// This, like the other parse_*() functions, returns NULL if the escape
	// sequence is incomplete.  Otherwise, it returns a pointer to the byte
	// after the escape sequence.

	char c;

	// Arguments.
	static const int max_args = 20;
	int args[max_args];
	int num_args = 0;
	for (int i = 0; i < max_args; ++i)
		args[i] = 0;
	while (true) {
		if (p >= end)
			return nullptr;
		c = *p;
		if (c >= '0' && c <= '9') {
			if (num_args < max_args) {
				args[num_args] *= 10;
				args[num_args] += c - '0';
				}
			p += 1;
			}
		else if (c == ';') {
			num_args += 1;
			p += 1;
			}
		else if (c >= 0x30 && c <= 0x3F) {
			// Valid, but we ignore it.
			p += 1;
			}
		else
			break;
		}

	// "Intermediate bytes".
	// We ignore these.
	while (true) {
		if (p >= end)
			return nullptr;
		c = *p;
		if (c >= 0x20 && c <= 0x2F)
			p += 1;
		else
			break;
		}

	// Last character tells us what to do.
	if (p >= end)
		return nullptr;
	c = *p++;
	switch (c) {
		case 'A':
			// Cursor up.
			current_line -= args[0] ? args[0] : 1;
			if (current_line < first_line)
				current_line = first_line;
			break;

		case 'B':
			// Cursor down.
			current_line += args[0] ? args[0] : 1;
			if (current_line > last_line)
				current_line = last_line;
			break;

		case 'C':
			// Cursor forward.
			current_column += args[0] ? args[0] : 1;
			//*** TODO: beyond end of line.
			break;

		case 'D':
			// Cursor back.
			current_column -= args[0] ? args[0] : 1;
			if (current_column < 0)
				current_column = 0;
			break;

		default:
			// This is either unimplemented or invalid.
			break;
		}

	return p;
}


const char* History::parse_dcs(const char* p, const char* end)
{
	const char* sequence_end = parse_st_string(p, end);
	if (sequence_end == nullptr)
		return nullptr;

	//*** TODO
	return sequence_end;
}


const char* History::parse_osc(const char* p, const char* end)
{
	const char* sequence_end = parse_st_string(p, end, true);
	if (sequence_end == nullptr)
		return nullptr;

	//*** TODO
	return sequence_end;
}


const char* History::parse_st_string(const char* p, const char* end, bool can_end_with_bel)
{
	while (p < end) {
		char c = *p++;
		if (c == '\x1B') {
			if (p >= end)
				return nullptr;
			if (*p++ == '\\') {
				// Got ST; string is complete.
				return p;
				}
			}
		else if (c == '\a' && can_end_with_bel)
			return p;
		}

	// Incomplete string.
	return nullptr;
}






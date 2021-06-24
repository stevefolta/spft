#include "History.h"
#include "Line.h"
#include "Colors.h"
#include "UTF8.h"
#include "Terminal.h"
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
	#include <stdio.h>
#endif


History::History()
	: cursor_enabled(true), terminal(nullptr)
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
	top_margin = 0;
	bottom_margin = -1;
	alternate_screen_top_line = -1;
}


History::~History()
{
	for (int64_t i = 0; i < capacity; ++i)
		delete lines[i];
	delete[] lines;
}


void History::set_lines_on_screen(int new_lines_on_screen)
{
	if (is_in_alternate_screen()) {
		// We assume the "alternate screen" is a "full screen" mode.  Make sure
		// it's got the exact correct number of lines.
		if (new_lines_on_screen > lines_on_screen) {
			// Adding lines.
			int delta = new_lines_on_screen - lines_on_screen;
			for (int i = delta; i > 0; --i)
				allocate_new_line();
			if (bottom_margin >= 0)
				bottom_margin += delta;
			}
		else {
			// (Potentially) deleting lines.
			last_line -= lines_on_screen - new_lines_on_screen;
			if (current_line > last_line)
				current_line = last_line;
			}
		}

	lines_on_screen = new_lines_on_screen;
}


int64_t History::num_lines()
{
	return last_line;
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
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
				printf("- Unimplemented DEL.\n");
#endif
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
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
							printf("- Unimplemented escape: %c%.*s\n", c, (int) (parse_end - p), p);
#endif
							break;
						default:
							// Unimplemented.
							// We assume only one character follows the ESC.
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
							printf("- Unimplemented escape: %c.\n", c);
#endif
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

			case '\r':
				current_column = 0;
				break;

			case '\n':
				{
				bool needs_scroll =
					(top_margin > 0 && current_line >= last_line) ||
					(bottom_margin >= 0 &&
					 current_line == calc_screen_top_line() + bottom_margin);
				if (needs_scroll) {
					int64_t screen_top = calc_screen_top_line();
					scroll_up(screen_top + top_margin, screen_top + bottom_margin, 1);
					update_at_end_of_line();
					}
				else if (current_line >= last_line)
					new_line();
				else {
					current_line += 1;
					update_at_end_of_line();
					}
				}
				break;

			case '\b':
				if (current_column > 0) {
					current_column -= 1;
					at_end_of_line = false;
					}
				break;

			case '\a':
				// BEL.  Ignore.
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
		cur_line->replace_characters(
			current_column, start, end - start, current_style);
		}
	current_column += UTF8::num_characters(start, end - start);
	if (!at_end_of_line)
		update_at_end_of_line();
}


void History::new_line()
{
	allocate_new_line();
	current_line = last_line;
	at_end_of_line = true;
}


void History::allocate_new_line()
{
	last_line += 1;
	if (last_line >= capacity) {
		// History is full, we'll recycle the previous first line if necessary.
		// (It might not be necessary because window resizing can delete lines
		// from the bottom.)
		int last_line_index = line_index(last_line);
		lines[last_line_index]->clear();
		if (last_line_index == first_line_index) {
			first_line += 1;
			first_line_index += 1;
			if (first_line_index >= capacity)
				first_line_index = 0;
			}
		}
	else {
		// We may not have allocated the Line yet.
		if (lines[last_line] == nullptr)
			lines[last_line] = new Line();
		else
			lines[last_line]->clear();
		}
}


void History::ensure_current_line()
{
	while (last_line < current_line)
		allocate_new_line();
}


void History::ensure_current_column()
{
	// If we moved beyond the end of the line, add some spaces.
	Line* cur_line = line(current_line);
	int cur_length = cur_line->num_characters();
	if (current_column > cur_length) {
		cur_line->append_spaces(current_column - cur_length, current_style);
		at_end_of_line = true;
		}
	else if (current_column == cur_length)
		at_end_of_line = true;
}


void History::update_at_end_of_line()
{
	at_end_of_line = (current_column >= line(current_line)->num_characters());
}


const char* History::parse_csi(const char* p, const char* end)
{
	// This, like the other parse_*() functions, returns NULL if the escape
	// sequence is incomplete.  Otherwise, it returns a pointer to the byte
	// after the escape sequence.

	char c;
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
	const char* escape_start = p;
#endif

	// Arguments.
	static const int max_args = 20;
	int args[max_args];
	int num_args = 0;
	for (int i = 0; i < max_args; ++i)
		args[i] = 0;
	bool private_code = false;
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
		else if (c == '?') {
			private_code = true;
			p += 1;
			}
		else if (c >= 0x30 && c <= 0x3F) {
			// Valid, but we ignore it.
			p += 1;
			}
		else
			break;
		}
	// There's always one more arg than there are semicolons.  This implies
	// there's always at least one arg.
	num_args += 1;

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
	if (!private_code)
	switch (c) {
		case 'A':
			// Cursor up.
			{
			current_line -= args[0] ? args[0] : 1;
			int64_t screen_top_line = calc_screen_top_line();
			if (current_line < screen_top_line)
				current_line = screen_top_line;
			update_at_end_of_line();
			}
			break;

		case 'B':
			// Cursor down.
			{
			current_line += args[0] ? args[0] : 1;
			int64_t screen_bottom_line = calc_screen_bottom_line();
			if (current_line > screen_bottom_line)
				current_line = screen_bottom_line;
			ensure_current_line();
			update_at_end_of_line();
			}
			break;

		case 'C':
			// Cursor forward.
			{
			current_column += args[0] ? args[0] : 1;
			ensure_current_column();
			}
			break;

		case 'D':
			// Cursor back.
			current_column -= args[0] ? args[0] : 1;
			if (current_column < 0)
				current_column = 0;
			at_end_of_line = false;
			break;

		case 'H':
			// Cursor Position.
			current_line = calc_screen_top_line() + (args[0] ? args[0] - 1 : 0);
			current_column = args[1] ? args[1] - 1 : 0;
			ensure_current_line();
			ensure_current_column();
			update_at_end_of_line();
			break;

		case 'J':
			// Erase in Display.
			if (args[0] == 0)
				clear_to_end_of_screen();
			else if (args[0] == 1)
				clear_to_beginning_of_screen();
			else if (args[0] == 2 || args[0] == 3)
				clear_screen();
			break;

		case 'K':
			// Erase in Line.
			{
			Line* cur_line = line(current_line);
			if (args[0] == 0) {
				cur_line->clear_to_end_from(current_column);
				at_end_of_line = true;
				}
			else if (args[0] == 1) {
				cur_line->clear_from_beginning_to(current_column);
				cur_line->prepend_spaces(current_column, current_style);
				update_at_end_of_line();
				}
			else if (args[0] == 2) {
				cur_line->clear();
				if (current_column > 0)
					cur_line->prepend_spaces(current_column, current_style);
				at_end_of_line = true;
				}
			}
			break;

		case 'L':
			// Insert blank lines (IL).
			insert_lines(args[0] ? args[0] : 1);
			break;

		case 'P':
			// Delete Character (DCH).
			line(current_line)->delete_characters(current_column, args[0] ? args[0] : 1);
			update_at_end_of_line();
			break;

		case 'm':
			// Select Graphic Rendition (SGR).
			switch (args[0]) {
				case 0:
					current_style.reset();
					break;
				case 30: case 31: case 32: case 33:
				case 34: case 35: case 36: case 37:
					// Set forground color.
					current_style.foreground_color = args[0] - 30;
					break;
				case 38:
					// Set foreground color.
					if (args[1] == 5)
						current_style.foreground_color = args[2];
					else if (args[1] == 2) {
						current_style.foreground_color =
							Colors::true_color_bit | args[2] << 16 | args[3] << 8 | args[4];
						}
					break;
				case 40: case 41: case 42: case 43:
				case 44: case 45: case 46: case 47:
					// Set background color.
					current_style.background_color = args[0] - 40;
					break;
				case 48:
					// Set background color.
					if (args[1] == 5)
						current_style.background_color = args[2];
					else if (args[1] == 2) {
						current_style.background_color =
							Colors::true_color_bit | args[2] << 16 | args[3] << 8 | args[4];
						}
					break;
				}
			break;

		case 'n':
			if (args[0] == 6) {
				// Device Status Report (DSR).
				char report[32];
				sprintf(
					report, "\x1B[%d;%dR",
					(int) (current_line - calc_screen_top_line() + 1),
					current_column + 1);
				terminal->send(report);
				}
			else
				goto unimplemented;
			break;

		case 'r':
			// Set scroll margins (DECSTBM).
			top_margin = args[0] ? args[0] - 1 : 0;
			bottom_margin = args[1] ? args[1] - 1 : -1;
			if (top_margin >= bottom_margin) {
				// Invalid; reset them.
				top_margin = 0;
				bottom_margin = -1;
				}
			break;

		default:
		unimplemented:
			// This is either unimplemented or invalid.
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
			printf("- Unimplemented CSI: %.*s\n", (int) (p - escape_start), escape_start);
#endif
			break;
		}

	// Private codes.
	else {
		switch (c) {
			case 'h':
				// Set Mode (SM).
				for (int i = 0; i < num_args; ++i) {
					bool handled = set_private_mode(args[i], true);
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
					if (!handled)
						printf("- Unimplemented set mode: ?%d\n", args[i]);
#endif
					}
				break;

			case 'l':
				// Reset Mode (RM).
				for (int i = 0; i < num_args; ++i) {
					bool handled = set_private_mode(args[i], false);
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
					if (!handled)
						printf("- Unimplemented reset mode: ?%d\n", args[i]);
#endif
					}
				break;

			default:
				// This is either unimplemented or invalid.
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
				printf("- Unimplemented CSI: %.*s\n", (int) (p - escape_start), escape_start);
#endif
				break;
			}
		}

	return p;
}


const char* History::parse_dcs(const char* p, const char* end)
{
	const char* sequence_end = parse_st_string(p, end);
	if (sequence_end == nullptr)
		return nullptr;

	//*** TODO
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
	printf("- Unimplemented DCS: %.*s\n", (int) (sequence_end - p), p);
#endif
	return sequence_end;
}


const char* History::parse_osc(const char* p, const char* end)
{
	const char* sequence_end = parse_st_string(p, end, true);
	if (sequence_end == nullptr)
		return nullptr;

	//*** TODO
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
	printf("- Unimplemented OSC: %.*s\n", (int) (sequence_end - p), p);
#endif
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


bool History::set_private_mode(int mode, bool set)
{
	switch (mode) {
		case 25:
			cursor_enabled = set;
			break;

		case 1049:
			// Alternate screen.
			if (set)
				enter_alternate_screen();
			else
				exit_alternate_screen();
			break;

		default:
			return false;
		}

	return true;
}


int64_t History::calc_screen_top_line()
{
	int screen_top_line = last_line - lines_on_screen + 1;
	if (screen_top_line < 0)
		screen_top_line = 0;
	return screen_top_line;
}


int64_t History::calc_screen_bottom_line()
{
	// Usually "last_line", except early on...
	if (last_line < lines_on_screen)
		return lines_on_screen - 1;
	return last_line;
}


void History::clear_to_end_of_screen()
{
	line(current_line)->clear_to_end_from(current_column);
	for (int64_t which_line = current_line + 1; which_line <= last_line; ++which_line)
		line(which_line)->clear();
}


void History::clear_to_beginning_of_screen()
{
	Line* cur_line = line(current_line);
	cur_line->clear_from_beginning_to(current_column);
	cur_line->prepend_spaces(current_column, current_style);
	for (int64_t which_line = calc_screen_top_line(); which_line < current_line; ++which_line)
		line(which_line)->clear();
}


void History::clear_screen()
{
	for (int64_t which_line = calc_screen_top_line(); which_line <= last_line; ++which_line)
		line(which_line)->clear();
}


void History::insert_lines(int num_lines)
{
	int64_t screen_top_line = calc_screen_top_line();
	int64_t bottom_scroll_line =
		bottom_margin < 0 ? calc_screen_bottom_line() : screen_top_line + bottom_margin;
	int max_scroll = bottom_scroll_line - current_line + 1;
	if (num_lines > max_scroll)
		num_lines = max_scroll;

	// Move and erase the lines.
	for (int dest_line = bottom_scroll_line; dest_line >= current_line; --dest_line) {
		int dest_index = line_index(dest_line);
		int64_t src_line = dest_line - num_lines;
		if (src_line < current_line) {
			if (lines[dest_index])
				lines[dest_index]->clear();
			}
		else {
			int src_index = line_index(src_line);
			delete lines[dest_index];
			lines[dest_index] = lines[src_index];
			lines[src_index] = new Line();
			}
		}

	update_at_end_of_line();
}


void History::scroll_up(int64_t top_scroll_line, int64_t bottom_scroll_line, int num_lines)
{
	// Move and erase the lines.
	for (int dest_line = top_scroll_line; dest_line <= bottom_scroll_line; ++dest_line) {
		int dest_index = line_index(dest_line);
		int64_t src_line = dest_line + num_lines;
		if (src_line > bottom_scroll_line) {
			if (lines[dest_index])
				lines[dest_index]->clear();
			}
		else {
			int src_index = line_index(src_line);
			delete lines[dest_index];
			lines[dest_index] = lines[src_index];
			lines[src_index] = new Line();
			}
		}
}


void History::enter_alternate_screen()
{
	if (alternate_screen_top_line >= 0)
		return;

	// mlterm replaces the bottom lines with the alternate screen, but we add
	// new lines instead.  This allows you to see the entire main screen when
	// scrolling back.  We don't save the alternate screen after exiting it.

	// Save state.
	main_screen_current_line = current_line;
	main_screen_current_column = current_column;
	main_screen_top_margin = top_margin;
	main_screen_bottom_margin = bottom_margin;

	// Create the new lines.
	alternate_screen_top_line = last_line + 1;
	current_column = 0;
	for (int i = 0; i < lines_on_screen; ++i)
		allocate_new_line();

	// Reset state.
	current_line = alternate_screen_top_line;
	current_column = 0;
	top_margin = 0;
	bottom_margin = -1;
	update_at_end_of_line();
}


void History::exit_alternate_screen()
{
	if (alternate_screen_top_line < 0)
		return;

	// Delete the last screen's lines.
	last_line = alternate_screen_top_line - 1;

	// Restore state.
	current_line = main_screen_current_line;
	current_column = main_screen_current_column;
	top_margin = main_screen_top_margin;
	bottom_margin = main_screen_bottom_margin;
	alternate_screen_top_line = -1;
	update_at_end_of_line();
}






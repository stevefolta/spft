#include "History.h"
#include "Line.h"
#include "Colors.h"
#include "UTF8.h"
#include "Terminal.h"
#include "TermWindow.h"
#include "ElasticTabs.h"
#include <sstream>
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
	#include <stdio.h>
#endif


History::History() :
	cursor_enabled(true), use_bracketed_paste(false),
	application_cursor_keys(false),
	terminal(nullptr)
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
	current_elastic_tabs = nullptr;
	g0_character_set = 'B';
	insert_mode = false;
	auto_wrap = settings.default_auto_wrap;
	characters_per_line = 80;
}


History::~History()
{
	for (int i = 0; i < capacity; ++i)
		delete lines[i];
	delete[] lines;

	if (current_elastic_tabs)
		current_elastic_tabs->release();
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


void History::set_characters_per_line(int new_characters_per_line)
{
	characters_per_line = new_characters_per_line;
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
			case '\x7F': 	// DEL.
			case '\x00': 	// NULL
			case '\x05': 	// ENQ
			case '\x11': 	// DC1 / XON
			case '\x12': 	// DC2
			case '\x13': 	// DC3 / XOFF
			case '\x14': 	// DC4
				// Ignore all of these.
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
						case 'M':
							// Reverse Index.
							{
							if (current_line == calc_screen_top_line() + top_margin)
								insert_lines(1);
							else
								current_line -= 1;
							}
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
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
					printf("- Unimplemented escape: %c.\n", c);
#endif
					}
				else if (c >= 0x30 && c <= 0x3F) {
					// "Fp" escape sequence ("private use").
					if (c == '7') {
						// Save Cursor (DECSC).
						saved_line = current_line - calc_screen_top_line();
						saved_column = current_column;
						}
					else if (c == '8') {
						// Restore Cursor (DECRC).
						if (current_line >= 0 && current_column >= 0) {
							current_line = calc_screen_top_line() + saved_line;
							current_column = saved_column;
							ensure_current_line();
							ensure_current_column();
							update_at_end_of_line();
							}
						}
					else {
						// We assume only one character follows the ESC.
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
						printf("- Unimplemented escape: %c.\n", c);
#endif
						}
					}
				else if (c >= 0x20 && c <= 0x2F) {
					// "nF" escape sequence.
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
					run_start += 1;  // Skip the ESC.
					switch (run_start[0]) {
						case '(':
							if (p > run_start + 1) {
								g0_character_set = run_start[1];
								current_style.line_drawing = (g0_character_set == '0');
								}
							break;

						default:
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
							printf("- Unimplemented escape: %.*s\n", (int) (p - run_start), run_start);
#endif
							break;
						}
					}
				break;

			case '\r':
				current_column = 0;
				at_end_of_line = false;
				break;

			case '\n':
				next_line();
				break;

			case '\b':
				if (current_column > 0) {
					current_column -= 1;
					at_end_of_line = false;
					}
				break;

			case '\t':
				{
				Line* cur_line = line(current_line);
				if (at_end_of_line) {
					cur_line->append_tab(current_style);
					// Just need to make sure "current_elastic_tabs" gets enough
					// columns.
					characters_added();
					}
				else {
					cur_line->replace_character_with_tab(current_column, current_style);
					// Could be splitting a column.  Trigger a full recalculation of
					// the columns.
					characters_deleted();
					}
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
				if (g0_character_set == '0') {
					// DEC Special Character and Line Drawing Set.
					std::string translated_chars = translate_line_drawing_chars(run_start, p);
					add_characters(
						translated_chars.data(),
						translated_chars.data() + translated_chars.size());
					}
				else
					add_characters(run_start, p);
				break;

			unfinished_run:
				return run_start - input;
				break;
			}
		}

	return p - input;
}


void History::add_characters(const char* start, const char* end)
{
	if (auto_wrap) {
		int num_new_characters = UTF8::num_characters(start, end - start);
		while (current_column + num_new_characters > characters_per_line) {
			int chars_to_add = characters_per_line - current_column;
			if (at_end_of_line || !insert_mode) {
				int num_bytes = UTF8::bytes_for_n_characters(start, end - start, chars_to_add);
				add_to_current_line(start, start + num_bytes);
				start += num_bytes;
				next_line();
				current_column = 0;
				update_at_end_of_line();
				num_new_characters -= chars_to_add;
				}
			else {
				// In insert mode.
				// TODO: split the line.
				// Instead, for now, we just insert the characters.
				add_to_current_line(start, end);
				return;
				}
			}
		}

	if (end > start)
		add_to_current_line(start, end);
}


void History::add_to_current_line(const char* start, const char* end)
{
	Line* cur_line = line(current_line);
	if (at_end_of_line)
		cur_line->append_characters(start, end - start, current_style);
	else if (insert_mode) {
		cur_line->insert_characters(
			current_column, start, end - start, current_style);
		}
	else {
		cur_line->replace_characters(
			current_column, start, end - start, current_style);
		}
	current_column += UTF8::num_characters(start, end - start);
	characters_added();
	if (!at_end_of_line)
		update_at_end_of_line();
}


void History::next_line()
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
		Line* cur_line = line(current_line);
		if (cur_line->elastic_tabs)
			cur_line->elastic_tabs->release();
		cur_line->elastic_tabs = current_elastic_tabs;
		if (current_elastic_tabs)
			current_elastic_tabs->acquire();
		update_at_end_of_line();
		}
}


void History::new_line()
{
	allocate_new_line();
	current_line = last_line;
	line(current_line)->elastic_tabs = current_elastic_tabs;
	if (current_elastic_tabs)
		current_elastic_tabs->acquire();
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
		Line* line = lines[last_line_index];
		line->fully_clear();

		// If we took "first_line", update that.
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
			lines[last_line]->fully_clear();
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
		Style default_style;
		cur_line->append_spaces(current_column - cur_length, default_style);
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
#if defined(PRINT_UNIMPLEMENTED_ESCAPES) || defined(DUMP_CSIS)
	const char* escape_start = p;
#endif

	// Arguments.
	Arguments args;
	p = args.parse(p, end);
	if (p == nullptr)
		return nullptr;

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
	if (args.private_code_type == 0)
	switch (c) {
		case '@':
			// Insert blank characters (ICH).
			{
			int num_blanks = args.args[0] ? args.args[0] : 1;
			std::string blanks(num_blanks, ' ');
			line(current_line)->insert_characters(
				current_column, blanks.data(), num_blanks, current_style);
			at_end_of_line = false;
			characters_added();
			}
			break;

		case 'A':
		case 'F':
			// Cursor up (CUU) / Cursor Prev Line (CPL).
			{
			current_line -= args.args[0] ? args.args[0] : 1;
			int64_t screen_top_line = calc_screen_top_line();
			if (is_in_alternate_screen()) {
				if (current_line < screen_top_line)
					current_line = screen_top_line;
				}
			else {
				if (current_line < first_line)
					current_line = first_line;
				}
			if (c == 'F')
				current_column = 0;
			update_at_end_of_line();
			}
			break;

		case 'B':
		case 'E':
		case 'e':
			// Cursor down (CUD) / Cursor Next Line (CNL) / Line Position Relative
			// (VPR).
			{
			current_line += args.args[0] ? args.args[0] : 1;
			int64_t screen_bottom_line = calc_screen_bottom_line();
			if (current_line > screen_bottom_line)
				current_line = screen_bottom_line;
			if (c == 'E')
				current_column = 0;
			ensure_current_line();
			update_at_end_of_line();
			}
			break;

		case 'C':
			// Cursor forward.
			{
			current_column += args.args[0] ? args.args[0] : 1;
			ensure_current_column();
			}
			break;

		case 'D':
			// Cursor back.
			current_column -= args.args[0] ? args.args[0] : 1;
			if (current_column < 0)
				current_column = 0;
			at_end_of_line = false;
			break;

		case 'G':
			// Cursor Character Absolute (CHA).
			current_column = args.args[0] ? args.args[0] - 1 : 0;
			ensure_current_column();
			update_at_end_of_line();
			break;

		case 'H':
		case 'f':
			// Cursor Position (CUP).
			// 'f' is the obsolete "Horizontal and Vertical Position (HVP)", but
			// there are still applications that send it.
			current_line =
				calc_screen_top_line() + (args.args[0] ? args.args[0] - 1 : 0);
			current_column = args.args[1] ? args.args[1] - 1 : 0;
			ensure_current_line();
			ensure_current_column();
			update_at_end_of_line();
			break;

		case 'J':
			// Erase in Display.
			if (args.args[0] == 0)
				clear_to_end_of_screen();
			else if (args.args[0] == 1)
				clear_to_beginning_of_screen();
			else if (args.args[0] == 2 || args.args[0] == 3)
				clear_screen();
			update_at_end_of_line();
			break;

		case 'K':
			// Erase in Line.
			{
			Line* cur_line = line(current_line);
			if (args.args[0] == 0) {
				cur_line->clear_to_end_from(current_column);
				at_end_of_line = true;
				}
			else if (args.args[0] == 1) {
				cur_line->clear_from_beginning_to(current_column);
				cur_line->prepend_spaces(current_column, current_style);
				update_at_end_of_line();
				}
			else if (args.args[0] == 2) {
				cur_line->clear();
				if (current_column > 0)
					cur_line->prepend_spaces(current_column, current_style);
				at_end_of_line = true;
				}
			characters_deleted();
			}
			break;

		case 'L':
			// Insert blank lines (IL).
			insert_lines(args.args[0] ? args.args[0] : 1);
			break;

		case 'M':
			// Delete lines (DL).
			delete_lines(args.args[0] ? args.args[0] : 1);
			break;

		case 'P':
			// Delete Character (DCH).
			line(current_line)->delete_characters(current_column, args.args[0] ? args.args[0] : 1);
			update_at_end_of_line();
			characters_deleted();
			break;

		case 'S':
			// Scroll up (SU).
			// This scrolls the whole screen (or at least the scrolling region).
			{
			int effective_bottom_margin = bottom_margin;
			if (effective_bottom_margin < 0)
				effective_bottom_margin = lines_on_screen - 1;
			int64_t top_line = calc_screen_top_line();
			scroll_up(
				top_line + top_margin, top_line + effective_bottom_margin,
				args.args[0] ? args.args[0] : 1);
			}
			break;

		case 'T':
			// Scroll down (SD).
			scroll_down(
				args.args[0] ? args.args[0] : 1,
				calc_screen_top_line() + top_margin);
			break;

		case 'X':
			// Erase Character(s) (ECH).
			// This appears to mean replacing them with spaces, unlike DCH which
			// actually deletes characters.
			{
			int num_blanks = args.args[0] ? args.args[0] : 1;
			std::string blanks(num_blanks, ' ');
			line(current_line)->replace_characters(
				current_column, blanks.data(), num_blanks, current_style);
			at_end_of_line = false;
			characters_deleted();
			}
			break;

		case 'd':
			// Line Position Absolute (VPA).
			{
			current_line =
				calc_screen_top_line() + (args.args[0] ? args.args[0] - 1 : 0);
			int64_t top_line = calc_screen_top_line();
			if (current_line < top_line)
				current_line = top_line;
			else {
				int64_t bottom_line = calc_screen_bottom_line();
				if (current_line > bottom_line)
					current_line = bottom_line;
				}
			ensure_current_line();
			ensure_current_column();
			update_at_end_of_line();
			}
			break;

		case 'h':
			switch (args.args[0]) {
				case 4:
					insert_mode = true;
					break;
				default:
					goto unimplemented;
				}
			break;

		case 'l':
			switch (args.args[0]) {
				case 4:
					insert_mode = false;
					break;
				default:
					goto unimplemented;
				}
			break;

		case 'm':
			// Select Graphic Rendition (SGR).
			if (args.num_args == 0) {
				// Default to at least one arg (which will have the default value
				// of zero).
				args.num_args = 1;
				}
			for (int which_arg = 0; which_arg < args.num_args; ++which_arg) {
				switch (args.args[which_arg]) {
					case 0:
						current_style.reset();
						if (g0_character_set == '0')
							current_style.line_drawing = true;
						break;
					case 1:
						current_style.bold = true;
						break;
					case 3:
						current_style.italic = true;
						break;
					case 4:
						current_style.underlined = true;
						break;
					case 7:
						current_style.inverse = true;
						break;
					case 8:
						current_style.invisible = true;
						break;
					case 9:
						current_style.crossed_out = true;
						break;
					case 21:
						current_style.doubly_underlined = true;
						break;
					case 22:
						current_style.bold = false;
						break;
					case 23:
						current_style.italic = false;
						break;
					case 24:
						current_style.underlined = current_style.doubly_underlined = false;
						break;
					case 27:
						current_style.inverse = false;
						break;
					case 28:
						current_style.invisible = false;
						break;
					case 29:
						current_style.crossed_out = false;
						break;
					case 30: case 31: case 32: case 33:
					case 34: case 35: case 36: case 37:
						// Set foreground color.
						current_style.foreground_color = args.args[which_arg] - 30;
						break;
					case 90: case 91: case 92: case 93:
					case 94: case 95: case 96: case 97:
						// Set high-intensity foreground color.
						current_style.foreground_color = args.args[which_arg] - 90 + 8;
						break;
					case 38:
						// Set foreground color.
						which_arg += 1;
						if (args.args[which_arg] == 5) {
							which_arg += 1;
							current_style.foreground_color = args.args[which_arg];
							}
						else if (args.args[which_arg] == 2) {
							current_style.foreground_color =
								Colors::true_color_bit |
								args.args[which_arg + 1] << 16 |
								args.args[which_arg + 2] << 8 |
								args.args[which_arg + 3];
							which_arg += 3;
							}
						break;
					case 39:
						// Set foreground color to default.
						current_style.foreground_color = settings.default_foreground_color;
						break;
					case 40: case 41: case 42: case 43:
					case 44: case 45: case 46: case 47:
						// Set background color.
						current_style.background_color = args.args[which_arg] - 40;
						break;
					case 100: case 101: case 102: case 103:
					case 104: case 105: case 106: case 107:
						// Set high-intensity background color.
						current_style.background_color = args.args[which_arg] - 100 + 8;
						break;
					case 48:
						// Set background color.
						which_arg += 1;
						if (args.args[which_arg] == 5) {
							which_arg += 1;
							current_style.background_color = args.args[which_arg];
							}
						else if (args.args[which_arg] == 2) {
							current_style.background_color =
								Colors::true_color_bit |
								args.args[which_arg + 1] << 16 |
								args.args[which_arg + 2] << 8 |
								args.args[which_arg + 3];
							which_arg += 3;
							}
						break;
					case 49:
						// Set background color to default.
						current_style.background_color = settings.default_background_color;
						break;
					}
				}
			break;

		case 'n':
			if (args.args[0] == 6) {
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
			top_margin = args.args[0] ? args.args[0] - 1 : 0;
			bottom_margin = args.args[1] ? args.args[1] - 1 : -1;
			if (top_margin >= bottom_margin) {
				// Invalid; reset them.
				top_margin = 0;
				bottom_margin = -1;
				}
			if (bottom_margin >= lines_on_screen - 1)
				bottom_margin = -1;
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
	else if (args.private_code_type == '?') {
		switch (c) {
			case 'h':
				// Set Mode (SM).
				set_private_modes(&args, true);
				break;

			case 'l':
				// Reset Mode (RM).
				set_private_modes(&args, false);
				break;

			default:
				// This is either unimplemented or invalid.
#ifdef PRINT_UNIMPLEMENTED_ESCAPES
				printf("- Unimplemented CSI: %.*s\n", (int) (p - escape_start), escape_start);
#endif
				break;
			}
		}

#ifdef DUMP_CSIS
	printf("- %.*s\n", (int) (p - escape_start), escape_start);
#endif
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

	// Trim the "<ESC>\" or "<BEL>" off the end.
	sequence_end -= (sequence_end[-1] == '\a' ? 1 : 2);

	// Get the argument.
	int arg = -1;
	while (p < sequence_end) {
		char c = *p++;
		if (c == ';')
			break;
		else if (c >= '0' || c <= '9') {
			if (arg < 0)
				arg = 0;
			arg = (arg * 10) + (c - '0');
			}
		else {
			// Invalid character.
			return sequence_end;
			}
		}

	// Set Text Parameters.

	if (arg == 0 || arg == 2) {
		// Change Window Title.
		std::string title(p, sequence_end - p);
		window->set_title(title.c_str());
		return sequence_end;
		}

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


void History::set_private_modes(Arguments* args, bool set)
{
	for (int i = 0; i < args->num_args; ++i) {
		switch (args->args[i]) {
			case 1:
				// DECCKM.
				application_cursor_keys = set;
				break;

			case 7:
				auto_wrap = set;
				break;

			case 12:
				// Cursor blinking.
				// We don't support this currently, so we're ignoring it.
				break;

			case 25:
				// Show cursor (DECTCEM).
				cursor_enabled = set;
				break;

			case 1049:
				// Alternate screen.
				if (set)
					enter_alternate_screen();
				else
					exit_alternate_screen();
				break;

			case 2004:
				use_bracketed_paste = set;
				break;

			case 5001:
				if (set)
					start_elastic_tabs();
				else
					end_elastic_tabs();
				break;
			case 5002:
				if (set)
					start_elastic_tabs(args->args[++i]);
				else
					end_elastic_tabs(args->args[++i] == 1);
				break;

			default:
				printf("- Unimplemented set mode: ?%d\n", args->args[i]);
				break;
			}
		}
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
	scroll_down(num_lines, current_line);
}


void History::scroll_down(int num_lines, int64_t top_scroll_line)
{
	int64_t bottom_scroll_line =
		bottom_margin < 0 ?
		calc_screen_bottom_line() :
		calc_screen_top_line() + bottom_margin;
	int max_scroll = bottom_scroll_line - top_scroll_line + 1;
	if (num_lines > max_scroll)
		num_lines = max_scroll;

	// Move and erase the lines.
	for (int dest_line = bottom_scroll_line; dest_line >= top_scroll_line; --dest_line) {
		int dest_index = line_index(dest_line);
		int64_t src_line = dest_line - num_lines;
		if (src_line < top_scroll_line) {
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


void History::delete_lines(int num_lines)
{
	if (is_in_alternate_screen() || bottom_margin >= 0) {
		// Scrolling a particular area.
		int effective_bottom_margin = bottom_margin;
		if (effective_bottom_margin < 0) {
			// Must be in alternate screen.
			effective_bottom_margin = lines_on_screen - 1;
			}
		scroll_up(
			current_line, calc_screen_top_line() + effective_bottom_margin,
			num_lines);
		}
	else {
		scroll_up(current_line, last_line, num_lines);
		last_line -= num_lines;
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



void History::start_elastic_tabs(int num_right_columns)
{
	end_elastic_tabs();

	current_elastic_tabs = new ElasticTabs(num_right_columns);
	current_elastic_tabs->acquire();
	Line* cur_line = line(current_line);
	if (cur_line->elastic_tabs)
		cur_line->elastic_tabs->release();
	cur_line->elastic_tabs = current_elastic_tabs;
	current_elastic_tabs->acquire();
}


void History::end_elastic_tabs(bool include_current_line)
{
	if (current_elastic_tabs == nullptr)
		return;

	// The current cursor line will not be part of the group of elastic tabbed
	// lines (unless include_current_line is true).
	if (!include_current_line) {
		Line* cur_line = line(current_line);
		if (cur_line->elastic_tabs == current_elastic_tabs) {
			current_elastic_tabs->release();
			cur_line->elastic_tabs = nullptr;
			}
		}
	current_elastic_tabs->release();
	current_elastic_tabs = nullptr;
}


void History::characters_added()
{
	if (current_elastic_tabs == nullptr)
		return;

	current_elastic_tabs->is_dirty = true;
	if (current_line < current_elastic_tabs->first_dirty_line)
		current_elastic_tabs->first_dirty_line = current_line;
}


void History::characters_deleted()
{
	if (current_elastic_tabs == nullptr)
		return;

	current_elastic_tabs->is_dirty = true;
	current_elastic_tabs->first_dirty_line = 0;
}


const char* History::Arguments::parse(const char* p, const char* end)
{
	char c;

	// Clear.
	num_args = 0;
	for (int i = 0; i < max_args; ++i)
		args[i] = 0;
	private_code_type = 0;

	// Parse.
	bool arg_started = false;
	while (true) {
		if (p >= end)
			return nullptr;
		c = *p;
		if (c >= '0' && c <= '9') {
			if (num_args < max_args) {
				args[num_args] *= 10;
				args[num_args] += c - '0';
				arg_started = true;
				}
			p += 1;
			}
		else if (c == ';') {
			num_args += 1;
			arg_started = false;
			p += 1;
			}
		else if (c == '?' || c == '<' || c == '=' || c == '>') {
			private_code_type = c;
			p += 1;
			}
		else if (c >= 0x30 && c <= 0x3F) {
			// Valid, but we ignore it.
			p += 1;
			}
		else
			break;
		}
	if (arg_started)
		num_args += 1;

	return p;
}


std::string History::translate_line_drawing_chars(const char* start, const char* end)
{
	static const char* translation[] = {
		"\u2518", "\u2510", "\u250C", "\u2514", "\u253C", "",
		"", "\u2500", "", "", "\u251C", "\u2524", "\u2534", "\u252C", "\u2502",
		};

	std::stringstream result;
	for (const char* p = start; p < end; ++p) {
		char c = *p;
		if (c >= 0x6A && c <= 0x78)
			result << translation[c - 0x6A];
		else
			result << c;
		}
	return result.str();
}





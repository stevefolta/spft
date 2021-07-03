#include "TermWindow.h"
#include "Terminal.h"
#include "History.h"
#include "Line.h"
#include "Run.h"
#include "Colors.h"
#include "ElasticTabs.h"
#include "Settings.h"
#include "UTF8.h"
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <stdio.h>


TermWindow::TermWindow()
	: pixmap(0), xft_draw(0)
{
	top_line = -1;
	selecting_state = NotSelecting;
	last_click_time.tv_sec = 0;
	closed = false;

	history = new History();
	terminal = new Terminal(history);
	history->set_terminal(terminal);

	display = XOpenDisplay(nullptr);
	if (display == nullptr)
		throw std::runtime_error("Can't open display");
	screen = XDefaultScreen(display);
	Visual* visual = XDefaultVisual(display, screen);
	colors.init(display);

	attributes.background_pixel = WhitePixel(display, screen);
	attributes.border_pixel = BlackPixel(display, screen);
	attributes.bit_gravity = NorthWestGravity;
	attributes.event_mask =
		FocusChangeMask | KeyPressMask | KeyReleaseMask |
		ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;

	//*** TODO
	int x = 0;
	int y = 0;
	width = 700;
	height = 500;

	Window root_window = XRootWindow(display, screen);
	window =
		XCreateWindow(
			display, root_window,
			x, y, width, height,
			0, XDefaultDepth(display, screen), InputOutput, visual,
			CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask /*** | CWColorMap ***/,
			&attributes);

	XGCValues gc_values;
	memset(&gc_values, 0, sizeof(gc_values));
	gc_values.graphics_exposures = False;
	gc = XCreateGC(display, root_window, GCGraphicsExposures, &gc_values);

	// Fonts.
	setup_fonts();

	// Create pixmap, etc.
	// Need to have the xft_fonts before we do this.
	resized(width, height);

	// Cursor.
	Cursor cursor = XCreateFontCursor(display, XC_xterm);
	XDefineCursor(display, window, cursor);

	// Window manager.
	wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	wm_name_atom = XInternAtom(display, "_NET_WM_NAME", False);
	target_atom = XInternAtom(display, "UTF8_STRING", 0);
	if (target_atom == None)
		target_atom = XA_STRING;
	XSetWMProtocols(display, window, &wm_delete_window_atom, 1);
	set_title(settings.window_title.c_str());

	XMapWindow(display, window);
	XSync(display, False);

	// Wait for window mapping.
	int new_width = width;
	int new_height = height;
	while (true) {
		XEvent event;
		XNextEvent(display, &event);
		// st's source insists XFilterEvent() is necessary because of XOpenIM.
		if (XFilterEvent(&event, None))
			continue;
		if (event.type == MapNotify)
			break;
		if (event.type == ConfigureNotify) {
			new_width = event.xconfigure.width;
			new_height = event.xconfigure.height;
			}
		}
	resized(new_width, new_height);
}


TermWindow::~TermWindow()
{
	delete terminal;
	delete history;

	cleanup_fonts();
	XftDrawDestroy(xft_draw);
	XFreeGC(display, gc);
	XFreePixmap(display, pixmap);
	XDestroyWindow(display, window);
}


void TermWindow::setup_fonts()
{
	FcResult result;

	// Regular.
	FcPattern* pattern =
		FcNameParse((const FcChar8*) settings.font_spec.c_str());
	XftDefaultSubstitute(display, screen, pattern);
	FcPattern* match = FcFontMatch(NULL, pattern, &result);
	xft_fonts[0] = XftFontOpenPattern(display, match);
	FcPatternDestroy(match);
	if (xft_fonts[0] == nullptr)
		throw std::runtime_error("Couldn't open the font.");

	// Italic.
	// (We do this first because we'll be clobbering the weight later.)
	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	match = FcFontMatch(NULL, pattern, &result);
	xft_fonts[2] = XftFontOpenPattern(display, match);
	FcPatternDestroy(match);
	if (xft_fonts[2] == nullptr) {
		fprintf(stderr, "Couldn't open italic font.");
		xft_fonts[2] = xft_fonts[0];
		}

	// Bold italic.
	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	match = FcFontMatch(NULL, pattern, &result);
	xft_fonts[3] = XftFontOpenPattern(display, match);
	FcPatternDestroy(match);
	if (xft_fonts[3] == nullptr) {
		fprintf(stderr, "Couldn't open bold italic font.");
		xft_fonts[3] = xft_fonts[0];
		}

	// Bold.
	// (We do this first because we'll be clobbering the weight later.)
	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	match = FcFontMatch(NULL, pattern, &result);
	xft_fonts[1] = XftFontOpenPattern(display, match);
	FcPatternDestroy(match);
	if (xft_fonts[1] == nullptr) {
		fprintf(stderr, "Couldn't open bold font.");
		xft_fonts[1] = xft_fonts[0];
		}

	FcPatternDestroy(pattern);
}


void TermWindow::cleanup_fonts()
{
	for (int i = 3; i >= 0; --i) {
		if (i == 0 || xft_fonts[i] != xft_fonts[0])
			XftFontClose(display, xft_fonts[i]);
		xft_fonts[i] = nullptr;
		}
}


bool TermWindow::is_done()
{
	return closed || terminal->is_done();
}


void TermWindow::tick()
{
	// Wait until we get something.
	if (!XPending(display)) {
		fd_set fds;
		FD_ZERO(&fds);
		int xfd = XConnectionNumber(display);
		FD_SET(xfd, &fds);
		int terminal_fd = terminal->get_terminal_fd();
		FD_SET(terminal_fd, &fds);
		int max_fd = xfd;
		if (terminal_fd > max_fd)
			max_fd = terminal_fd;
		int result = select(max_fd + 1, &fds, NULL, NULL, NULL);
		if (result < 0 && errno != EINTR)
			throw std::runtime_error("select() failed");

		if (FD_ISSET(terminal_fd, &fds)) {
			terminal->tick();
			draw();
			}
		}

	while (XPending(display)) {
		XEvent event;
		XNextEvent(display, &event);
		if (XFilterEvent(&event, None))
			continue;
		switch (event.type) {
			case ConfigureNotify:
				resized(event.xconfigure.width, event.xconfigure.height);
				break;
			case Expose:
				draw();
				break;
			case ClientMessage:
				if ((Atom) event.xclient.data.l[0] == wm_delete_window_atom) {
					terminal->hang_up();
					closed = true;
					}
				break;
			case KeyPress:
				key_down(&event.xkey);
				break;
			case ButtonPress:
				mouse_button_down(&event.xbutton);
				break;
			case MotionNotify:
				mouse_moved(&event.xmotion);
				break;
			case ButtonRelease:
				mouse_button_up(&event.xbutton);
				break;
			case SelectionRequest:
				selection_requested(&event.xselectionrequest);
				break;
			case PropertyNotify:
				property_changed(&event);
				break;
			case SelectionNotify:
				received_selection(&event);
				break;
			}
		}
}


void TermWindow::draw()
{
	// Clear the background.
	XftDrawRect(
		xft_draw, colors.xft_color(settings.default_background_color),
		0, 0, width, height);

	// Draw the lines.
	int64_t num_lines = displayed_lines();
	int64_t effective_top_line = calc_effective_top_line();
	int64_t last_line = effective_top_line + num_lines - 1;
	if (last_line > history->get_last_line())
		last_line = history->get_last_line();
	int y = xft_fonts[0]->ascent;
	int64_t current_line = history->get_current_line();
	for (int64_t which_line = effective_top_line; which_line <= last_line; ++which_line) {
		bool line_contains_cursor =
			which_line == current_line && history->cursor_enabled;
		bool line_contains_inversity_change =
			line_contains_cursor ||
			which_line == selection_start.line || which_line == selection_end.line;
		Line* line = history->line(which_line);

		// Handle elastic tabs.
		if (line->elastic_tabs) {
			if (line->elastic_tabs->is_dirty)
				recalc_elastic_columns(which_line);
			}
		int cur_column_width = 0;
		int which_column = 0;

		// Draw the runs in the line.
		int x = 0;
		int chars_drawn = 0; 	// Only updated for the line with the cursor or a selection change on it.
		int current_column = history->get_current_column();
		for (auto run: *line) {
			int num_bytes = strlen(run->bytes());
			XGlyphInfo glyph_info;

			uint32_t foreground_color = run->style.foreground_color;
			uint32_t background_color = run->style.background_color;
			if (run->style.inverse) {
				foreground_color = run->style.background_color;
				background_color = run->style.foreground_color;
				}

			bool run_contains_inversity_change = false;
			if (line_contains_inversity_change) {
				int run_chars = run->num_characters();
				int run_end = chars_drawn + run_chars;
				bool run_contains_cursor =
					line_contains_cursor &&
					current_column >= chars_drawn &&
					current_column < run_end;
				if (run_contains_cursor)
					run_contains_inversity_change = true;
				else {
					run_contains_inversity_change =
						(which_line == selection_start.line &&
						 selection_start.column > chars_drawn &&
						 selection_start.column < run_end) ||
						(which_line == selection_end.line &&
						 selection_end.column > chars_drawn &&
						 selection_end.column < run_end);
					}
				}

			// Tab.
			if (run->is_tab) {
				int tab_width = 0;
				if (line->elastic_tabs) {
					int column_width =
						line->elastic_tabs->column_widths[which_column];
					tab_width =
						column_width - cur_column_width + settings.column_separation;
					}
				else {
					tab_width = settings.tab_width - (x % settings.tab_width);
					// Make sure we always have at least the width of a space.
					XftTextExtentsUtf8(
						display, xft_font_for(run->style),
						(const FcChar8*) " ", 1, &glyph_info);
					if (tab_width < glyph_info.xOff)
						tab_width += settings.tab_width;
					}

				// Draw the tab.
				SelectionPoint draw_point(which_line, chars_drawn);
				bool inversity =
					(line_contains_cursor && current_column == chars_drawn) ^
					(draw_point >= selection_start && draw_point < selection_end);
				uint32_t cur_background = (inversity ? foreground_color : background_color);
				if (cur_background != settings.default_background_color) {
					XftDrawRect(
						xft_draw, colors.xft_color(cur_background),
						x, y - xft_fonts[0]->ascent,
						tab_width, xft_fonts[0]->height);
					}
				x += tab_width;
				chars_drawn += 1;

				// Start the next column.
				which_column += 1;
				cur_column_width = 0;

				continue;
				}

			// The simple case: drawing the entire run at once.
			XftFont* xft_font = xft_font_for(run->style);
			if (!run_contains_inversity_change) {
				XftTextExtentsUtf8(
					display, xft_font,
					(const FcChar8*) run->bytes(), num_bytes, &glyph_info);

				SelectionPoint draw_point(which_line, chars_drawn);
				bool inversity =
					(line_contains_cursor && current_column == chars_drawn) ^
					(draw_point >= selection_start && draw_point < selection_end);

				// Draw the run background (if it's not the default).
				uint32_t cur_background = (inversity ? foreground_color : background_color);
				if (cur_background != settings.default_background_color) {
					XftDrawRect(
						xft_draw, colors.xft_color(cur_background),
						x, y - xft_fonts[0]->ascent,
						glyph_info.xOff, xft_fonts[0]->height);
					}

				// Draw the run text.
				XftDrawStringUtf8(
					xft_draw,
					colors.xft_color(inversity? background_color : foreground_color),
					xft_font,
					x, y,
					(const FcChar8*) run->bytes(), num_bytes);

				x += glyph_info.xOff; 	// Not "width".  It'd be nice if Xft were documented...
				cur_column_width += glyph_info.xOff;
				if (line_contains_inversity_change)
					chars_drawn += run->num_characters();
				}

			// The more complicated case: inversity changes within the run
			// (because it contains the cursor or the start or end of the selection).
			else {
				int run_chars = run->num_characters();
				int run_end_char = chars_drawn + run_chars;
				const char* subrun_start_byte = run->bytes();
				while (chars_drawn < run_end_char) {
					int next_flip = run_end_char;
					if (line_contains_cursor) {
						if (current_column == chars_drawn)
							next_flip = current_column + 1;
						else if (current_column > chars_drawn && current_column < run_end_char)
							next_flip = current_column;
						}
					if (which_line == selection_start.line) {
						if (selection_start.column > chars_drawn && selection_start.column < next_flip)
							next_flip = selection_start.column;
						}
					if (which_line == selection_end.line) {
						if (selection_end.column > chars_drawn && selection_end.column < next_flip)
							next_flip = selection_end.column;
						}

					// Figure out the subrun.
					int subrun_num_chars = next_flip - chars_drawn;
					int subrun_num_bytes =
						UTF8::bytes_for_n_characters(
							subrun_start_byte,
							num_bytes - (subrun_start_byte - run->bytes()),
							subrun_num_chars);
					XftTextExtentsUtf8(
						display, xft_font,
						(const FcChar8*) subrun_start_byte, subrun_num_bytes,
						&glyph_info);
					int subrun_width = glyph_info.xOff;

					SelectionPoint draw_point(which_line, chars_drawn);
					bool inversity =
						(line_contains_cursor && current_column == chars_drawn) ^
						(draw_point >= selection_start && draw_point < selection_end);

					// Draw the background.
					uint32_t cur_background = (inversity ? foreground_color : background_color);
					XftDrawRect(
						xft_draw, colors.xft_color(cur_background), 
						x, y - xft_fonts[0]->ascent,
						subrun_width, xft_fonts[0]->height);

					// Characters.
					uint32_t cur_foreground = (inversity ? background_color : foreground_color);
					XftDrawStringUtf8(
						xft_draw, colors.xft_color(cur_foreground), xft_font,
						x, y,
						(const FcChar8*) subrun_start_byte, subrun_num_bytes);

					chars_drawn += subrun_num_chars;
					subrun_start_byte += subrun_num_bytes;
					x += subrun_width;
					cur_column_width += subrun_width;
					}
				}
			}

		// Draw the cursor if it's at the end of the line.
		bool draw_eol_cursor =
			which_line == current_line && current_column >= chars_drawn &&
			history->cursor_enabled;
		if (draw_eol_cursor) {
			// "x" is already at the end of the line.
			XGlyphInfo glyph_info;
			XftTextExtentsUtf8(
				display, xft_fonts[0],
				(const FcChar8*) " ", 1,
				&glyph_info);
			XftDrawRect(
				xft_draw, colors.xft_color(settings.default_foreground_color), 
				x, y - xft_fonts[0]->ascent,
				glyph_info.xOff, xft_fonts[0]->height);
			}

		y += xft_fonts[0]->height;
		}

	// Copy to the screen.
	XCopyArea(
		display, pixmap, window, gc, 0, 0, width, height, 0, 0);
	XFlush(display);
}


void TermWindow::resized(unsigned int new_width, unsigned int new_height)
{
	if (pixmap && new_width == width && new_height == height)
		return;

	// Set up drawing (new pixmap etc.).
	width = new_width;
	height = new_height;
	if (pixmap)
		XFreePixmap(display, pixmap);
	pixmap = XCreatePixmap(display, window, width, height, DefaultDepth(display, screen));
	if (xft_draw)
		XftDrawChange(xft_draw, pixmap);
	else {
		xft_draw =
			XftDrawCreate(
				display, pixmap,
				XDefaultVisual(display, screen),
				XDefaultColormap(display, screen));
		}
	XSetForeground(display, gc, attributes.background_pixel);
	XFillRectangle(display, pixmap, gc, 0, 0, width, height);

	screen_size_changed();
}


void TermWindow::set_title(const char* title)
{
	XTextProperty property;
	Xutf8TextListToTextProperty(display, (char**) &title, 1, XUTF8StringStyle, &property);
	XSetWMName(display, window, &property);
	XSetTextProperty(display, window, &property, wm_name_atom);
	XFree(property.value);
}


void TermWindow::screen_size_changed()
{
	int lines_on_screen = height / xft_fonts[0]->height;

	history->set_lines_on_screen(lines_on_screen);

	// Notify the terminal.
	XGlyphInfo glyph_info;
	XftTextExtentsUtf8(
		display, xft_fonts[0],
		(const FcChar8*) "M", 1,
		&glyph_info);
	terminal->notify_resize(
		width / (glyph_info.xOff * settings.average_character_width),
		lines_on_screen,
		width, height);
}


void TermWindow::key_down(XKeyEvent* event)
{
	char buffer[64];
	KeySym keySym = 0;
	int length = XLookupString(event, buffer, sizeof(buffer), &keySym, NULL);

	// Shift-PgUp/PgDown/Break.
	if ((event->state & ShiftMask) != 0) {
		int64_t num_lines = displayed_lines();
		int64_t half_page = num_lines / 2 + 1;
		if (keySym == XK_Page_Up) {
			if (top_line < 0) {
				// Scroll up from the bottom.
				if (history->get_last_line() < num_lines) {
					// All the lines still fit on the screen; don't change anything.
					return;
					}
				top_line = history->get_last_line() - num_lines + 1 - half_page;
				if (top_line < history->get_first_line())
					top_line = history->get_first_line();
				}
			else {
				// Scroll up from where we are.
				top_line -= half_page;
				if (top_line < history->get_first_line())
					top_line = history->get_first_line();
				}
			draw();
			return;
			}
		else if (keySym == XK_Page_Down) {
			if (top_line >= 0) {
				top_line += half_page;
				if (top_line > history->get_last_line() - num_lines) {
					// We've reached the end.
					top_line = -1;
					}
				draw();
				}
			return;
			}
		else if (keySym == XK_Insert) {
			settings.read_settings_files();
			cleanup_fonts();
			setup_fonts();
			screen_size_changed();
			draw();
			return;
			}
		}

	// Special keys that XLookupString() doesn't handle.
	static const KeyMapping key_mappings[] = {
		{ XK_Up, AnyModKey, "\x1B[A" },
		{ XK_Down, AnyModKey, "\x1B[B" },
		{ XK_Left, AnyModKey, "\x1B[D" },
		{ XK_Right, AnyModKey, "\x1B[C" },
		{ XK_Home, AnyModKey, "\x1B[H" },
		{ XK_End, AnyModKey, "\x1B[F" },
		{ XK_Prior, 0, "\x1B[5~" }, 	// Page up.
		{ XK_Next, 0, "\x1B[6~" }, 	// Page down.
		{ XK_Insert, AnyModKey, "\x1B[2~" },
		{ XK_Delete, AnyModKey, "\x1B[3~" },
		{ XK_ISO_Left_Tab, ShiftMask, "\x1B[Z" },
		{ XK_F1, AnyModKey, "\x1BOP" },
		{ XK_F2, AnyModKey, "\x1BOQ" },
		{ XK_F3, AnyModKey, "\x1BOR" },
		{ XK_F4, AnyModKey, "\x1BOS" },
		{ XK_F5, AnyModKey, "\x1B[15~" },
		{ XK_F6, AnyModKey, "\x1B[17~" },
		{ XK_F7, AnyModKey, "\x1B[18~" },
		{ XK_F8, AnyModKey, "\x1B[19~" },
		{ XK_F9, AnyModKey, "\x1B[20~" },
		{ XK_F10, AnyModKey, "\x1B[21~" },
		{ XK_F11, AnyModKey, "\x1B[23~" },
		{ XK_F12, AnyModKey, "\x1B[24~" },
		};
	static const KeyMapping application_cursor_key_mappings[] = {
		{ XK_Up, AnyModKey, "\x1BOA" },
		{ XK_Down, AnyModKey, "\x1BOB" },
		{ XK_Left, AnyModKey, "\x1BOD" },
		{ XK_Right, AnyModKey, "\x1BOC" },
		{ XK_Home, AnyModKey, "\x1BOH" },
		{ XK_End, AnyModKey, "\x1BOF" },
		};
	#define ARRAY_SIZE(array)	(sizeof(array) / sizeof(array[0]))
	if (history->application_cursor_keys) {
		bool handled =
			check_special_key(
				keySym, event->state,
				application_cursor_key_mappings,
				ARRAY_SIZE(application_cursor_key_mappings));
		if (handled)
			return;
		}
	if (check_special_key(keySym, event->state, key_mappings, ARRAY_SIZE(key_mappings)))
		return;

	// The normal case is just to send what XLookupString() gave us.
	if (length > 0) {
		// Alt-key -> ESC key.
		if (length == 1 && (event->state & Mod1Mask) != 0) {
			buffer[1] = buffer[0];
			buffer[0] = '\x1B';
			length = 2;
			}
		terminal->send(buffer, length);
		scroll_to_bottom();
		}
}


bool TermWindow::check_special_key(
	KeySym key_sym, unsigned int state,
	const KeyMapping* key_mappings, int num_key_mappings)
{
	const KeyMapping* mappings_end = &key_mappings[num_key_mappings];
	for (const KeyMapping* mapping = key_mappings; mapping < mappings_end; ++mapping) {
		bool matches =
			key_sym == mapping->keySym &&
			(mapping->mask == AnyModKey || mapping->mask == (state & ~Mod2Mask));
		if (matches) {
			terminal->send(mapping->str);
			scroll_to_bottom();
			return true;
			}
		}

	return false;
}


void TermWindow::mouse_button_down(XButtonEvent* event)
{
	if (event->button == Button1) {
		// Which line is it on?
		int64_t num_lines = displayed_lines();
		int64_t effective_top_line = calc_effective_top_line();
		int64_t last_line = effective_top_line + num_lines - 1;
		if (last_line > history->get_last_line())
			last_line = history->get_last_line();
		selection_start.line = effective_top_line + event->y / xft_fonts[0]->height;
		if (selection_start.line < effective_top_line)
			selection_start.line = effective_top_line;
		else if (selection_start.line > last_line)
			selection_start.line = last_line;

		// Which column?
		selection_start.column = column_for_pixel(selection_start.line, event->x);

		// Set the new selection.
		selection_end.line = selection_start.line;
		selection_end.column = selection_start.column + 1;
		selecting_state = SelectingForward;

		// Handle double- and triple-clicks.
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		uint64_t last_click_ms =
			last_click_time.tv_sec * 1000 + last_click_time.tv_nsec / 1e6;
		uint64_t now_ms = now.tv_sec * 1000 + now.tv_nsec / 1e6;
		if (now_ms - last_click_ms < settings.double_click_ms) {
			if (selecting_by == SelectingByChar) {
				selecting_by = SelectingByWord;
				selection_start = start_of_word(selection_start);
				selection_end = end_of_word(selection_start);
				}
			else if (selecting_by == SelectingByWord) {
				selecting_by = SelectingByLine;
				selection_start.column = 0;
				selection_end.column = INT_MAX;
				}
			}
		else
			selecting_by = SelectingByChar;
		last_click_time = now;

		draw();
		}

	else if (event->button == Button2) {
		paste();
		}
}


void TermWindow::mouse_moved(XMotionEvent* event)
{
	if (selecting_state == NotSelecting)
		return;

	// Autoscroll.
	if (event->y < 0) {
		top_line = calc_effective_top_line() - 1;
		if (top_line < history->get_first_line())
			top_line = history->get_first_line();
		}
	else if (event->y > displayed_lines() * xft_fonts[0]->height) {
		int new_top_line = calc_effective_top_line() + 1;
		if (new_top_line + displayed_lines() >= history->get_last_line())
			scroll_to_bottom();
		else
			top_line = new_top_line;
		}

	SelectionPoint cur_selection_start;
	if (selecting_state == SelectingBackward)
		cur_selection_start = selection_end;
	else
		cur_selection_start = selection_start;

	// Which line are we on?
	SelectionPoint mouse_position;
	int64_t num_lines = displayed_lines();
	int64_t effective_top_line = calc_effective_top_line();
	int64_t last_line = effective_top_line + num_lines - 1;
	if (last_line > history->get_last_line())
		last_line = history->get_last_line();
	mouse_position.line = effective_top_line + event->y / xft_fonts[0]->height;
	if (mouse_position.line < effective_top_line)
		mouse_position.line = effective_top_line;
	else if (mouse_position.line > last_line)
		mouse_position.line = last_line;

	// Which column?
	mouse_position.column = column_for_pixel(mouse_position.line, event->x);

	// Set the selection.
	if (mouse_position < cur_selection_start) {
		selection_start = mouse_position;
		selection_end = cur_selection_start;
		selecting_state = SelectingBackward;
		}
	else {
		selection_start = cur_selection_start;
		selection_end = mouse_position;
		selecting_state = SelectingForward;
		}
	if (selecting_by == SelectingByWord) {
		selection_start = start_of_word(selection_start);
		selection_end = end_of_word(selection_end);
		}
	else if (selecting_by == SelectingByLine) {
		selection_start.column = 0;
		selection_end.column = INT_MAX;
		}

	draw();
}


void TermWindow::mouse_button_up(XButtonEvent* event)
{
	if (event->button == Button1) {
		selecting_state = NotSelecting;

		// Build the selected text.
		std::stringstream text;
		for (int which_line = selection_start.line; which_line <= selection_end.line; ++which_line) {
			Line* line = history->line(which_line);
			if (line == nullptr)
				continue;
			text <<
				line->characters_from_to(
					which_line == selection_start.line ? selection_start.column : 0,
					which_line == selection_end.line ? selection_end.column : INT_MAX);
			if (which_line != selection_end.line || selecting_by == SelectingByLine)
				text << '\n';
			}
		selected_text = text.str();

		// Tell X we've got the selection.
		XSetSelectionOwner(display, XA_PRIMARY, window, event->time);
		if (XGetSelectionOwner(display, XA_PRIMARY) != window)
			selection_start.line = -1;
		}
}


void TermWindow::selection_requested(XSelectionRequestEvent* event)
{
	// We send the whole selection at once, even if it's big.  The ICCCM says we
	// should use the "large data transfer" protocol if the selection is larger
	// than XMaxRequestSize(display), but we don't do that yet.

	XSelectionEvent response;
	response.type = SelectionNotify;
	response.requestor = event->requestor;
	response.selection = event->selection;
	response.target = event->target;
	response.time = event->time;
	response.property = None; 	// Default: reject.

	Atom targets_atom = XInternAtom(display, "TARGETS", 0);
	if (event->target == targets_atom) {
		// Respond with the supported type.
		XChangeProperty(
			event->display, event->requestor, event->property,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char*) &target_atom, 1);
		response.property = event->property;
		}
	else if (event->target == target_atom || event->target == XA_STRING) {
		if (event->selection == XA_PRIMARY) {
			if (!selected_text.empty()) {
				Atom property = event->property;
				if (property == None)
					property = event->target;
				XChangeProperty(
					event->display, event->requestor, property, event->target,
					8, PropModeReplace,
					(unsigned char*) selected_text.data(), selected_text.size());
				response.property = event->property;
				}
			}
		else {
			// We don't currently handle other types of selection.
			return;
			}
		}

	// Send the reply.
	int result =
		XSendEvent(event->display, event->requestor, true, 0, (XEvent*) &response);
	if (!result)
		fprintf(stderr, "Error sending SelectionNotify event.\n");
}


void TermWindow::property_changed(XEvent* event)
{
	if (event->xproperty.state == PropertyNewValue && event->xproperty.atom == XA_PRIMARY) {
		// This is a continuation of a selection transfer.
		received_selection(event);
		}
}


void TermWindow::received_selection(XEvent* event)
{
	Atom property = None;
	if (event->type == SelectionNotify)
		property = event->xselection.property;
	else if (event->type == PropertyNotify)
		property = event->xproperty.atom;
	if (property == None)
		return;

	Atom incr_atom = XInternAtom(display, "INCR", 0);

	long offset = 0;
	unsigned long bytes_left = 0;
	do {
		// Get the next chunk of data.
		Atom type;
		int format;
		unsigned long num_items = 0;
		unsigned char* data;
		int result =
			XGetWindowProperty(
				display, window, property, offset, BUFSIZ / 4, False,
				AnyPropertyType,
				&type, &format, &num_items, &bytes_left, &data);
		if (result != Success) {
			fprintf(stderr, "Receiving clipboard data failed.\n");
			return;
			}

		if (event->type == PropertyNotify && num_items == 0 && bytes_left == 0) {
			// This is the signal that all data has been transferred.  We no
			// longer need to receive PropertyNotify events.
			attributes.event_mask &= ~PropertyChangeMask;
			XChangeWindowAttributes(display, window, CWEventMask, &attributes);
			}

		if (type == incr_atom) {
			// Multi-part transfer.
			// We need get the rest of the parts via PropertyNotify events.
			attributes.event_mask |= PropertyChangeMask;
			XChangeWindowAttributes(display, window, CWEventMask, &attributes);
			// Signal transfer start by deleting the property.
			XDeleteProperty(display, window, property);
			}

		// Convert '\n' to '\r'.
		unsigned long num_bytes = num_items * format / 8;
		unsigned char* p = data;
		unsigned char* end = data + num_bytes;
		for (; p < end; ++p) {
			if (*p == '\n')
				*p = '\r';
			}

		// Send to the client.
		if (history->use_bracketed_paste)
			terminal->send("\x1B[200~");
		terminal->send((const char*) data, num_bytes);
		if (history->use_bracketed_paste)
			terminal->send("\x1B[201~");

		XFree(data);
		// Offset is in 32-bit quantities, not bytes.
		offset += num_items * format / 32;
	} while (bytes_left > 0);

	// Delete the property to let the selection owner know we're ready for the
	// next chunk.
	XDeleteProperty(display, window, property);
}


void TermWindow::paste()
{
	XConvertSelection(
		display, XA_PRIMARY, target_atom, XA_PRIMARY, window, CurrentTime);
}


int TermWindow::column_for_pixel(int64_t which_line, int x)
{
	Line* line = history->line(which_line);
	int column = 0;
	int cur_column_width = 0;
	int which_column = 0;
	XGlyphInfo glyph_info;
	int initial_x = x;
	for (auto run: *line) {
		// Tab.
		if (run->is_tab) {
			int tab_width = 0;
			if (line->elastic_tabs) {
				int column_width =
					line->elastic_tabs->column_widths[which_column];
				tab_width =
					column_width - cur_column_width + settings.column_separation;
				}
			else {
				tab_width = settings.tab_width - ((initial_x - x) % settings.tab_width);
				// Make sure we always have at least the width of a space.
				XftTextExtentsUtf8(
					display, xft_font_for(run->style),
					(const FcChar8*) " ", 1, &glyph_info);
				if (tab_width < glyph_info.xOff)
					tab_width += settings.tab_width;
				}

			if (x < tab_width / 2)
				return column;
			x -= tab_width;
			column += 1;

			// Start the next column.
			which_column += 1;
			cur_column_width = 0;

			continue;
			}

		const char* p = run->bytes();
		const char* end = p + strlen(p);
		while (p < end) {
			int char_num_bytes = UTF8::bytes_for_n_characters(p, end - p, 1);
			XftTextExtentsUtf8(
				display, xft_font_for(run->style),
				(const FcChar8*) p, char_num_bytes, &glyph_info);
			if (x < glyph_info.xOff / 2)
				return column;
			x -= glyph_info.xOff;
			cur_column_width += glyph_info.xOff;
			column += 1;
			p += char_num_bytes;
			}
		}
	return column;
}


int64_t TermWindow::calc_effective_top_line()
{
	int64_t effective_top_line = top_line;
	if (effective_top_line < 0) {
		// Negative "top_line" means "bottom".
		effective_top_line = history->get_last_line() - displayed_lines() + 1;
		}
	if (effective_top_line < history->get_first_line())
		effective_top_line = history->get_first_line();
	return effective_top_line;
}


static inline bool is_word_char(const char* c)
{
	return
		settings.word_separator_characters.find(c) == std::string::npos &&
		settings.additional_word_separator_characters.find(c) == std::string::npos;
}


TermWindow::SelectionPoint TermWindow::start_of_word(SelectionPoint point)
{
	// If it's already not a word, stay there.
	char c[16];
	Line* line = history->line(point.line);
	line->get_character(point.column, c);
	if (!is_word_char(c))
		return point;

	// Search backwards for the first non-word character.
	while (point.column > 0) {
		line->get_character(point.column, c);
		if (!is_word_char(c)) {
			point.column += 1;
			return point;
			}
		point.column -= 1;
		}
	return point;
}


TermWindow::SelectionPoint TermWindow::end_of_word(SelectionPoint point)
{
	// Search forward for the first non-word character.
	Line* line = history->line(point.line);
	int line_num_characters = line->num_characters();
	while (point.column < line_num_characters) {
		char c[16];
		line->get_character(point.column, c);
		if (!is_word_char(c))
			return point;
		point.column += 1;
		}
	return point;
}


void TermWindow::recalc_elastic_columns(int64_t initial_line)
{
	Line* line = history->line(initial_line);
	if (line == nullptr || line->elastic_tabs == nullptr)
		return;
	ElasticTabs* elastic_tabs = line->elastic_tabs;

	// Recalculate from scratch using all lines in the group.
	elastic_tabs->column_widths.clear();

	// First, go backwards from initial_line.
	int64_t which_line = initial_line;
	for (; which_line >= history->get_first_line(); --which_line) {
		Line* cur_line = history->line(which_line);
		if (cur_line->elastic_tabs != elastic_tabs)
			break;
		if (which_line < elastic_tabs->first_dirty_line)
			break;
		update_elastic_columns_for(cur_line);
		}

	// Then go forward from the initial_line.
	which_line = initial_line + 1;
	for (; which_line <= history->get_last_line(); ++which_line) {
		Line* cur_line = history->line(which_line);
		if (cur_line->elastic_tabs != elastic_tabs)
			break;
		update_elastic_columns_for(cur_line);
		}

	elastic_tabs->undirtify();
}


void TermWindow::update_elastic_columns_for(Line* line)
{
	if (line == nullptr || line->elastic_tabs == nullptr)
		return;
	ElasticTabs* elastic_tabs = line->elastic_tabs;

	std::vector<int>::size_type which_column = 0;
	int column_width = 0;
	for (auto run: *line) {
		if (run->is_tab) {
			// Finish this column.
			if (elastic_tabs->column_widths.size() <= which_column)
				elastic_tabs->column_widths.push_back(column_width);
			else if (column_width > elastic_tabs->column_widths[which_column])
				elastic_tabs->column_widths[which_column] = column_width;
			which_column += 1;
			column_width = 0;
			continue;
			}

		// Incorporate this run into the current column width;
		XGlyphInfo glyph_info;
		const char* run_bytes = run->bytes();
		XftTextExtentsUtf8(
			display, xft_font_for(run->style),
			(const FcChar8*) run_bytes, strlen(run_bytes), &glyph_info);
		column_width += glyph_info.xOff;
		}

	// Finish the last column.
	if (elastic_tabs->column_widths.size() <= which_column)
		elastic_tabs->column_widths.push_back(column_width);
	else if (column_width > elastic_tabs->column_widths[which_column])
		elastic_tabs->column_widths[which_column] = column_width;
}




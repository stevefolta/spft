#include "TermWindow.h"
#include "Terminal.h"
#include "History.h"
#include "Line.h"
#include "Run.h"
#include "Colors.h"
#include "Settings.h"
#include "UTF8.h"
#include <X11/cursorfont.h>
#include <stdexcept>
#include <string.h>


TermWindow::TermWindow()
{
	top_line = -1;
	closed = false;

	history = new History();
	terminal = new Terminal(history);

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
	width = 600;
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
	pixmap =
		XCreatePixmap(
			display, window, width, height,
			DefaultDepth(display, screen));
	// Clear the pixmap.
	XSetForeground(display, gc, attributes.background_pixel);
	XFillRectangle(display, pixmap, gc, 0, 0, width, height);

	// Xft.
	xft_font = XftFontOpenName(display, screen, settings.font_spec.c_str());
	xft_draw = XftDrawCreate(display, pixmap, visual, XDefaultColormap(display, screen));

	// Cursor.
	Cursor cursor = XCreateFontCursor(display, XC_xterm);
	XDefineCursor(display, window, cursor);

	// Window manager.
	wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, window, &wm_delete_window_atom, 1);

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

	XftDrawDestroy(xft_draw);
	XftFontClose(display, xft_font);
	XFreeGC(display, gc);
	XFreePixmap(display, pixmap);
	XDestroyWindow(display, window);
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
	int64_t effective_top_line = top_line;
	if (effective_top_line < 0) {
		// Negative "top_line" means "bottom".
		effective_top_line = history->get_last_line() - num_lines + 1;
		}
	if (effective_top_line < history->get_first_line())
		effective_top_line = history->get_first_line();
	int64_t last_line = effective_top_line + num_lines - 1;
	if (last_line > history->get_last_line())
		last_line = history->get_last_line();
	int y = xft_font->ascent;
	int64_t current_line = history->get_current_line();
	for (int64_t which_line = effective_top_line; which_line <= last_line; ++which_line) {
		// Draw the runs in the line.
		int x = 0;
		int chars_drawn = 0; 	// Only updated for the line with the cursor on it.
		Line* line = history->line(which_line);
		int current_column = history->get_current_column();
		for (auto run: *line) {
			int num_bytes = strlen(run->bytes());
			XGlyphInfo glyph_info;
			XftTextExtentsUtf8(
				display, xft_font,
				(const FcChar8*) run->bytes(), num_bytes, &glyph_info);

			// Draw the run background (if it's not the default).
			if (run->style.background_color != settings.default_background_color) {
				XftDrawRect(
					xft_draw, colors.xft_color(run->style.background_color),
					x, y - xft_font->ascent,
					glyph_info.xOff, xft_font->height);
				}

			// Draw the run text.
			XftDrawStringUtf8(
				xft_draw, colors.xft_color(run->style.foreground_color), xft_font,
				x, y,
				(const FcChar8*) run->bytes(), num_bytes);

			// Draw the cursor if it's in the run.
			if (which_line == current_line) {
				int run_chars = run->num_characters();
				if (current_column >= chars_drawn && current_column < chars_drawn + run_chars) {
					int bytes_before_cursor =
						UTF8::bytes_for_n_characters(
							run->bytes(), num_bytes, current_column - chars_drawn);
					XGlyphInfo glyph_info; 	// Don't touch the outer one.
					XftTextExtentsUtf8(
						display, xft_font,
						(const FcChar8*) run->bytes(), bytes_before_cursor,
						&glyph_info);
					int cursor_x = x + glyph_info.xOff;
					const char* char_bytes = run->bytes() + bytes_before_cursor;
					int char_num_bytes =
						UTF8::bytes_for_n_characters(char_bytes, num_bytes - bytes_before_cursor, 1);
					XftTextExtentsUtf8(
						display, xft_font,
						(const FcChar8*) char_bytes, char_num_bytes,
						&glyph_info);
					int cursor_width = glyph_info.xOff;
					// Background.
					XftDrawRect(
						xft_draw, colors.xft_color(run->style.foreground_color), 
						cursor_x, y - xft_font->ascent,
						cursor_width, xft_font->height);
					// Character.
					XftDrawStringUtf8(
						xft_draw, colors.xft_color(run->style.background_color), xft_font,
						cursor_x, y,
						(const FcChar8*) char_bytes, char_num_bytes);
					}
				chars_drawn += run_chars;
				}

			x += glyph_info.xOff; 	// Not "width".  It'd be nice if Xft were documented...
			}

		// Draw the cursor if it's at the end of the line.
		if (which_line == current_line && current_column >= chars_drawn) {
			// "x" is already at the end of the line.
			XGlyphInfo glyph_info;
			XftTextExtentsUtf8(
				display, xft_font,
				(const FcChar8*) " ", 1,
				&glyph_info);
			XftDrawRect(
				xft_draw, colors.xft_color(settings.default_foreground_color), 
				x, y - xft_font->ascent,
				glyph_info.xOff, xft_font->height);
			}

		y += xft_font->height;
		}

	// Copy to the screen.
	XCopyArea(
		display, pixmap, window, gc, 0, 0, width, height, 0, 0);
	XFlush(display);
}


void TermWindow::resized(unsigned int new_width, unsigned int new_height)
{
	width = new_width;
	height = new_height;
	XFreePixmap(display, pixmap);
	pixmap = XCreatePixmap(display, window, width, height, DefaultDepth(display, screen));
	XftDrawChange(xft_draw, pixmap);
	XSetForeground(display, gc, attributes.background_pixel);
	XFillRectangle(display, pixmap, gc, 0, 0, width, height);
}


void TermWindow::key_down(XKeyEvent* event)
{
	char buffer[64];
	KeySym keySym = 0;
	int length = XLookupString(event, buffer, sizeof(buffer), &keySym, NULL);

	// Shift-PgUp/PgDown.
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
				return;
				}
			}
		}

	// Special keys that XLookupString() doesn't handle.
	struct KeyMapping {
		KeySym	keySym;
		const char*	str;
		};
	KeyMapping keyMappings[] = {
		{ XK_Up, "\x1B[A" },
		{ XK_Down, "\x1B[B" },
		{ XK_Left, "\x1B[D" },
		{ XK_Right, "\x1B[C" },
		};
	const KeyMapping* mappingsEnd = &keyMappings[sizeof(keyMappings) / sizeof(keyMappings[0])];
	for (KeyMapping* mapping = keyMappings; mapping < mappingsEnd; ++mapping) {
		if (keySym == mapping->keySym) {
			terminal->send(mapping->str);
			scroll_to_bottom();
			}
		}

	// The normal case is just to send what XLookupString() gave us.
	if (length > 0) {
		terminal->send(buffer, length);
		scroll_to_bottom();
		}
}




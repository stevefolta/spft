#include "TermWindow.h"
#include "Terminal.h"
#include "History.h"
#include "Line.h"
#include "Run.h"
#include "Settings.h"
#include <X11/cursorfont.h>
#include <stdexcept>
#include <string.h>


TermWindow::TermWindow()
{
	top_line = 0;
	closed = false;

	history = new History();
	terminal = new Terminal(history);

	display = XOpenDisplay(nullptr);
	if (display == nullptr)
		throw std::runtime_error("Can't open display");
	screen = XDefaultScreen(display);
	Visual* visual = XDefaultVisual(display, screen);

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
	XSetForeground(display, gc, attributes.background_pixel);
	XFillRectangle(display, pixmap, gc, 0, 0, width, height);

	XRenderColor fg_color;
	fg_color.alpha = 0xFFFF;
	fg_color.red = 0;
	fg_color.green = 0;
	fg_color.blue = 0;
	XftColor xft_color;
	XftColorAllocValue(
		display,
		XDefaultVisual(display, screen), XDefaultColormap(display, screen),
		&fg_color, &xft_color);

	if (top_line < history->get_first_line())
		top_line = history->get_first_line();
	int64_t num_lines = height / xft_font->height;
	int64_t last_line = top_line + num_lines - 1;
	if (last_line > history->get_last_line())
		last_line = history->get_last_line();
	int y = xft_font->ascent;
	for (int64_t which_line = top_line; which_line <= last_line; ++which_line) {
		int x = 0;
		Line* line = history->line(which_line);
		for (auto run: *line) {
			XftDrawStringUtf8(
				xft_draw, &xft_color, xft_font,
				x, y,
				(const FcChar8*) run->bytes(), strlen(run->bytes()));
			}
		y += xft_font->height;
		}

	XftColorFree(
		display,
		XDefaultVisual(display, screen), XDefaultColormap(display, screen),
		&xft_color);

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
	KeySym keySym;
	int length = XLookupString(event, buffer, sizeof(buffer), &keySym, NULL);
	if (length > 0)
		terminal->send(buffer, length);
}




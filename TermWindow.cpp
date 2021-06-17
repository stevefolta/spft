#include "TermWindow.h"
#include "Terminal.h"
#include "History.h"
#include <X11/cursorfont.h>
#include <stdexcept>
#include <string.h>


TermWindow::TermWindow()
{
	terminal = new Terminal();
	history = new History();

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

	xft_draw = XftDrawCreate(display, pixmap, visual, XDefaultColormap(display, screen));

	Cursor cursor = XCreateFontCursor(display, XC_xterm);
	XDefineCursor(display, window, cursor);

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

	const char* message = "Hello world...";
	history->add_input(message, strlen(message));
}


TermWindow::~TermWindow()
{
	delete terminal;
	delete history;

	XftDrawDestroy(xft_draw);
	XFreeGC(display, gc);
	XFreePixmap(display, pixmap);
	XDestroyWindow(display, window);
}


bool TermWindow::is_done()
{
	/*** TODO ***/
	return false;
}


void TermWindow::tick()
{
	// Wait until we get something.
	if (!XPending(display)) {
		fd_set fds;
		FD_ZERO(&fds);
		int xfd = XConnectionNumber(display);
		FD_SET(xfd, &fds);
		int result = select(xfd + 1, &fds, NULL, NULL, NULL);
		if (result < 0 && errno != EINTR)
			throw std::runtime_error("select() failed");
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
			}
		}
}


void TermWindow::draw()
{
	/***/
}


void TermWindow::resized(int width, int height)
{
	XFreePixmap(display, pixmap);
	pixmap = XCreatePixmap(display, window, width, height, DefaultDepth(display, screen));
	XftDrawChange(xft_draw, pixmap);
	XSetForeground(display, gc, attributes.background_pixel);
	XFillRectangle(display, pixmap, gc, 0, 0, width, height);
}




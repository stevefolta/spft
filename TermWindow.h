#ifndef TermWindow_h
#define TermWindow_h

// This would just be called "Window", but that conflicts with XWindow's type
// of the same name.

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>

class Terminal;
class History;


class TermWindow {
	public:
		TermWindow();
		~TermWindow();

		bool	is_done();
		void	tick();
		void	draw();
		void	resized(unsigned int new_width, unsigned int new_height);

	protected:
		bool	closed;
		Terminal* terminal;
		History* history;
		Display*	display;
		int screen;
		Window window;
		XSetWindowAttributes attributes;
		GC gc;
		Drawable pixmap;
		XftDraw* xft_draw;
		XftFont* xft_font;
		unsigned int width, height;

		int64_t top_line;

		Atom wm_delete_window_atom;

		void	key_down(XKeyEvent* event);
	};


#endif 	// !TermWindow_h


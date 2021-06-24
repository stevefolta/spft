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

		void	screen_size_changed();
		void	key_down(XKeyEvent* event);
		int	displayed_lines() { return height / xft_font->height; }
		void	scroll_to_bottom() { top_line = -1; }
	};


#endif 	// !TermWindow_h


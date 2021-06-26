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
		void	set_title(const char* title);

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
		int64_t first_selected_line, last_selected_line;
		int first_selected_column, selection_end_column;
		bool has_selection() { return first_selected_line >= 0; }
		enum {
			NotSelecting, SelectingForward, SelectingBackward,
			};
		int	selecting_state;

		Atom wm_delete_window_atom;
		Atom wm_name_atom;

		void	screen_size_changed();
		void	key_down(XKeyEvent* event);
		void	mouse_button_down(XButtonEvent* event);
		void	mouse_moved(XMotionEvent* event);
		void	mouse_button_up(XButtonEvent* event);
		int	displayed_lines() { return height / xft_font->height; }
		void	scroll_to_bottom() { top_line = -1; }

		int	column_for_pixel(int64_t which_line, int x);
		int64_t	calc_effective_top_line();
	};


#endif 	// !TermWindow_h


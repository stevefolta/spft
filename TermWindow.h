#ifndef TermWindow_h
#define TermWindow_h

// This would just be called "Window", but that conflicts with XWindow's type
// of the same name.

#include "Style.h"
#include "FontSet.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <string>
#include <time.h>
#include <stdint.h>

class Terminal;
class History;
class Line;


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
		FontSet* regular_font = nullptr;
		FontSet* line_drawing_font = nullptr;
		unsigned int width, height;
		double font_size_override = 0;

		int64_t top_line;

		struct SelectionPoint {
			int64_t	line;
			int	column;

			SelectionPoint()
				: line(-1), column(0) {}
			SelectionPoint(int64_t line_in, int column_in)
				: line(line_in), column(column_in) {}
			bool operator<(SelectionPoint& other) {
				return
					line < other.line ||
					(line == other.line && column < other.column);
				}
			bool	operator>=(SelectionPoint& other) {
				return !(*this < other);
				}
			};
		SelectionPoint selection_start, selection_end;
		bool has_selection() { return selection_start.line >= 0; }
		enum {
			NotSelecting, SelectingForward, SelectingBackward,
			};
		int	selecting_state;
		enum {
			SelectingByChar, SelectingByWord, SelectingByLine,
			};
		int	selecting_by;
		struct timespec	last_click_time;
		std::string selected_text;
		void	clear_selection() {
			selection_start.line = selection_end.line = -1;
			}

		Atom wm_delete_window_atom;
		Atom wm_name_atom;
		Atom target_atom;

		struct KeyMapping {
			KeySym	keySym;
			unsigned int	mask;
			const char*	str;
			};
		enum {
			AnyModKey = UINT_MAX,
			};
		bool	check_special_key(
			KeySym key_sym, unsigned int state,
			const KeyMapping* key_mappings, int num_key_mappings);

		XftFont*	xft_font_for(const Style& style) {
			return (style.line_drawing ? line_drawing_font : regular_font)->xft_font_for(style);
			}

		void	setup_fonts();
		void	cleanup_fonts();
		void	screen_size_changed();
		void	key_down(XKeyEvent* event);
		void	mouse_button_down(XButtonEvent* event);
		void	mouse_moved(XMotionEvent* event);
		void	mouse_button_up(XButtonEvent* event);
		void	selection_requested(XSelectionRequestEvent* event);
		void	property_changed(XEvent* event);
		void	received_selection(XEvent* event);
		int	displayed_lines() { return (height - 2 * settings.border) / regular_font->height(); }
		void	scroll_to_bottom() { top_line = -1; }
		void	decorate_run(Style style, int x, int width, int y);

		void	paste();

		int	column_for_pixel(int64_t which_line, int x);
		int64_t	calc_effective_top_line();
		SelectionPoint	start_of_word(SelectionPoint point);
		SelectionPoint	end_of_word(SelectionPoint point);

		// Elastic tabs.
		void	recalc_elastic_columns(int64_t initial_line);
		void	update_elastic_columns_for(Line* line);
	};


#endif 	// !TermWindow_h


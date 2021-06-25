#ifndef History_h
#define History_h

#include "Style.h"
#include <stdint.h>

class Line;
class Terminal;

// The History is the heart of the terminal.  It's the list of all the lines,
// and it gets the characters and interprets them to build those lines.


class History {
	public:
		History();
		~History();
		void	set_terminal(Terminal* new_terminal) { terminal = new_terminal; }

		void	set_lines_on_screen(int new_lines_on_screen);

		int64_t	num_lines();
		Line*	line(int64_t which_line) {
			return lines[line_index(which_line)];
			}

		int	add_input(const char* input, int length);
			// Returns number of bytes consumed.

		int64_t	get_first_line() { return first_line; }
		int64_t	get_last_line() { return last_line; }
		int64_t	get_current_line() { return current_line; }
		int	get_current_column() { return current_column; }
		bool	is_in_alternate_screen() { return alternate_screen_top_line >= 0; }
		bool	cursor_enabled;

	private:
		Style	current_style;
		bool	at_end_of_line;
		// Lines are numbered from the beginning of the session; line numbers are
		// never reused (that's why we use 64 bits for them).  "first_line_index"
		// is the index of "first_line" within the "lines" array.
		int64_t	capacity, first_line, first_line_index, last_line;
		int64_t	current_line;
		int	lines_on_screen;
		int	current_column;
		Line**	lines;
		Terminal*	terminal;
		int	top_margin, bottom_margin; 	// -1 bottom_margin means "bottom of screen"
		int64_t	alternate_screen_top_line; 	// -1: not in alternate screen.

		// Saved during alternate screen.
		int64_t	main_screen_current_line;
		int	main_screen_current_column;
		int	main_screen_top_margin, main_screen_bottom_margin;

		int	line_index(int64_t which_line) {
			return (first_line_index + (which_line - first_line)) % capacity;
			}

		void	add_to_current_line(const char* start, const char* end);
		void	new_line();
		void	allocate_new_line();
		void	ensure_current_line();
		void	ensure_current_column();
		void	update_at_end_of_line();

		const char*	parse_csi(const char* p, const char* end);
		const char*	parse_dcs(const char* p, const char* end);
		const char*	parse_osc(const char* p, const char* end);
		const char*	parse_st_string(const char* p, const char* end, bool can_end_with_bel = false);
		bool	set_private_mode(int mode, bool set); 	// false => unimplemented

		int64_t	calc_screen_top_line();
		int64_t	calc_screen_bottom_line();

		void	clear_to_end_of_screen();
		void	clear_to_beginning_of_screen();
		void	clear_screen();
		void	insert_lines(int num_lines);
		void	delete_lines(int num_lines);
		void	scroll_up(int64_t top_scroll_line, int64_t bottom_scroll_line, int num_lines);

		void	enter_alternate_screen();
		void	exit_alternate_screen();

		struct Arguments {
			enum {
				max_args = 20,
				};
			int	args[max_args];
			int	num_args;
			char	private_code_type;

			const char*	parse(const char* p, const char* end);
			};
	};


#endif 	// !History_h



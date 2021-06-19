#ifndef History_h
#define History_h

#include "Style.h"
#include <stdint.h>

class Line;


class History {
	public:
		History();
		~History();

		int64_t	num_lines();
		Line*	line(int64_t which_line);

		int	add_input(const char* input, int length);
			// Returns number of bytes consumed.

		int64_t	get_first_line() { return first_line; }
		int64_t	get_last_line() { return last_line; }

	private:
		Style	current_style;
		bool	at_end_of_line;
		// Lines are numbered from the beginning of the session; line numbers are
		// never reused (that's why we use 64 bits for them).  "first_line_index"
		// is the index of "first_line" within the "lines" array.
		int64_t	capacity, first_line, first_line_index, last_line;
		int64_t	current_line;
		int	current_column;
		Line**	lines;

		void	add_to_current_line(const char* start, const char* end);
		void	new_line();
		void	update_at_end_of_line();

		const char*	parse_csi(const char* p, const char* end);
		const char*	parse_dcs(const char* p, const char* end);
		const char*	parse_osc(const char* p, const char* end);
		const char*	parse_st_string(const char* p, const char* end, bool can_end_with_bel = false);
	};


#endif 	// !History_h



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

	private:
		Style	current_style;
		bool	at_end_of_line;
		int64_t	capacity, first_line, first_line_index, last_line;
		int64_t	current_line;
		Line**	lines;

		void	add_to_current_line(const char* start, const char* end);
	};


#endif 	// !History_h



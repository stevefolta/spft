#ifndef History_h
#define History_h

class History {
	public:
		History();

		int	num_lines();
		const char*	line(int which_line);

		int	add_input(const char* input, int length);
			// Returns number of bytes consumed.

	private:
		void	add_to_current_line(const char* start, const char* end);
	};


#endif 	// !History_h



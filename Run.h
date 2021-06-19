#ifndef Run_h
#define Run_h

#include "Style.h"

class Run {
	public:
		Run(Style style);
		Run(char* initial_characters, Style style);
			// Takes ownership of "initial characters", which must be null-terminated.
		~Run();

		Style	style;
		const char*	bytes() { return characters; }

		int	num_characters();
		void	append_characters(const char* new_chars, int num_bytes);
		void	append_spaces(int num_spaces);
		void	replace_characters(int column, const char* new_chars, int num_bytes);

		void	shorten_to(int new_length);
		void	delete_first_characters(int num_chars);
		void	delete_characters(int column, int num_chars);

	protected:
		char*	characters;
	};


#endif 	// !Run_h


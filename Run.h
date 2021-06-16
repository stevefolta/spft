#ifndef Run_h
#define Run_h

#include "Style.h"

class Run {
	public:
		Run(Style style);
		~Run();

		Style	style;
		const char*	bytes() { return characters; }

		int	num_characters();
		void	append_characters(const char* new_chars, int num_bytes);

	protected:
		char*	characters;
	};


#endif 	// !Run_h


#ifndef Line_h
#define Line_h

#include "Style.h"
#include <list>

class Run;

class Line {
	public:
		Line();
		~Line();

		void	append_characters(const char* bytes, int length, Style style);
		bool	empty()  {
			return runs.empty();
			}

	protected:
		std::list<Run*>	runs;
	};


#endif 	// !Line_h


#ifndef Line_h
#define Line_h

#include "Style.h"
#include <list>
#include <string>

class Run;

class Line {
	public:
		Line();
		~Line();

		bool	has_tabs;

		void	append_characters(const char* bytes, int length, Style style);
		void	replace_characters(int column, const char* bytes, int length, Style style);
		void	append_tab(Style style);
		void	replace_character_with_tab(int column, Style style);
		void	clear();
		bool	empty()  {
			return runs.empty();
			}
		int	num_characters();
		void	get_character(int column, char* char_out);
		std::string	characters_from_to(int start_column, int end_column);

		// As an collection, it gives the runs.
		std::list<Run*>::iterator	begin() { return runs.begin(); }
		std::list<Run*>::iterator end()	{ return runs.end(); }

		void	clear_to_end_from(int column);
		void	clear_from_beginning_to(int column);
		void	prepend_spaces(int num_spaces, Style style);
		void	append_spaces(int num_spaces, Style style);
		void	delete_characters(int column, int num_chars);

	protected:
		std::list<Run*>	runs;

		void	recalc_has_tabs();
	};


#endif 	// !Line_h


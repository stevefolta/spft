#ifndef ElasticTabs_h
#define ElasticTabs_h

#include <vector>
#include <stdint.h>


class ElasticTabs {
	public:
		ElasticTabs(int64_t initial_line) :
			first_line(initial_line), last_line(initial_line),
			columns_could_be_widened(false), columns_could_be_narrowed(false),
			first_dirty_line(INT64_MAX), last_dirty_line(-1)
			{}

		int64_t	first_line, last_line;
		std::vector<int>	column_widths;

		// Dirtiness.
		bool columns_could_be_widened;
		bool columns_could_be_narrowed;
		int64_t	first_dirty_line, last_dirty_line;
		void undirtify() {
			first_dirty_line = INT64_MAX;
			last_dirty_line = -1;
			columns_could_be_widened = columns_could_be_narrowed = false;
			}
	};


#endif 	// ElasticTabs_h



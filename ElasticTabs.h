#ifndef ElasticTabs_h
#define ElasticTabs_h

#include <vector>
#include <stdint.h>


class ElasticTabs {
	public:
		ElasticTabs(int64_t initial_line) :
			reference_count(0), is_dirty(false), first_dirty_line(INT64_MAX)
			{}

		std::vector<int>	column_widths;
		int	reference_count;

		void acquire() { reference_count += 1; }
		void release() {
			if (--reference_count <= 0)
				delete this;
			}

		// Dirtiness.
		bool is_dirty;
		int64_t first_dirty_line;
		void undirtify() {
			is_dirty = false;
			first_dirty_line = INT64_MAX;
			}
	};


#endif 	// ElasticTabs_h



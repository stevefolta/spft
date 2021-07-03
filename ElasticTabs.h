#ifndef ElasticTabs_h
#define ElasticTabs_h

#include <vector>
#include <stdint.h>


class ElasticTabs {
	public:
		ElasticTabs(int64_t initial_line) :
			reference_count(0), is_dirty(false)
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
		void undirtify() { is_dirty = false; }
	};


#endif 	// ElasticTabs_h



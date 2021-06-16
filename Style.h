#ifndef Style_h
#define Style_h

class Style {
	public:
		char	flags;

		Style() : flags(0) {}

		bool	operator==(Style& other) {
			return flags == other.flags;
			}
		bool	operator!=(Style& other) {
			return !(*this == other);
			}
	};


#endif 	// !Style_h


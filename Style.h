#ifndef Style_h
#define Style_h

class Style {
	public:
		char	flags;
		unsigned char	color;

		Style() : flags(0) {}

		bool	operator==(Style& other) {
			return flags == other.flags && color == other.color;
			}
		bool	operator!=(Style& other) {
			return !(*this == other);
			}
	};


#endif 	// !Style_h


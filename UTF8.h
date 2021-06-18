#ifndef UTF8_h
#define UTF8_h


class UTF8 {
	public:
		// These assume that the bytes are valid UTF8.
		static int	num_characters(const char* bytes, int length);
		static int	bytes_for_n_characters(const char* bytes, int length, int n);
	};


#endif 	// !UTF8_h


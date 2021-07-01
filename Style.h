#ifndef Style_h
#define Style_h

#include <stdint.h>
#include "Settings.h"


class Style {
	public:
		uint32_t	foreground_color, background_color;
		bool	inverse : 1;
		bool	bold : 1;
		bool	italic : 1;

		Style() :
			foreground_color(settings.default_foreground_color),
			background_color(settings.default_background_color),
			inverse(false), bold(false), italic(false)
		{}

		void	reset() {
			foreground_color = settings.default_foreground_color;
			background_color = settings.default_background_color;
			inverse = bold = italic = false;
			}

		bool	operator==(Style& other) {
			return
				foreground_color == other.foreground_color &&
				background_color == other.background_color &&
				inverse == other.inverse &&
				bold == other.bold && italic == other.italic;
			}
		bool	operator!=(Style& other) {
			return !(*this == other);
			}
	};


#endif 	// !Style_h


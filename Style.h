#ifndef Style_h
#define Style_h

#include <stdint.h>
#include "Settings.h"


class Style {
	public:
		char	flags;
		uint32_t	foreground_color, background_color;

		Style()
			: flags(0),
			foreground_color(settings.default_foreground_color),
			background_color(settings.default_background_color)
		{}

		void	reset() {
			flags = 0;
			foreground_color = settings.default_foreground_color;
			background_color = settings.default_background_color;
			}

		bool	operator==(Style& other) {
			return
				flags == other.flags &&
				foreground_color == other.foreground_color &&
				background_color == other.background_color;
			}
		bool	operator!=(Style& other) {
			return !(*this == other);
			}
	};


#endif 	// !Style_h


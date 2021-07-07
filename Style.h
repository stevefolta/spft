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
		bool	line_drawing: 1;
		bool	invisible : 1;
		bool	underlined : 1;
		bool	doubly_underlined : 1;
		bool	crossed_out : 1;

		Style() :
			foreground_color(settings.default_foreground_color),
			background_color(settings.default_background_color),
			inverse(false), bold(false), italic(false), line_drawing(false),
			invisible(false), underlined(false), doubly_underlined(false),
			crossed_out(false)
		{}

		void	reset() {
			foreground_color = settings.default_foreground_color;
			background_color = settings.default_background_color;
			inverse = bold = italic = line_drawing = false;
			invisible = underlined = doubly_underlined = crossed_out = false;
			}

		bool	operator==(Style& other) {
			return
				foreground_color == other.foreground_color &&
				background_color == other.background_color &&
				inverse == other.inverse &&
				bold == other.bold && italic == other.italic &&
				line_drawing == other.line_drawing &&
				invisible == other.invisible &&
				underlined == other.underlined &&
				doubly_underlined == other.doubly_underlined &&
				crossed_out == other.crossed_out;
			}
		bool	operator!=(Style& other) {
			return !(*this == other);
			}

		bool	has_decorations() {
			return underlined || doubly_underlined || crossed_out;
			}
	};


#endif 	// !Style_h


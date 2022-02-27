#pragma once

#include "Style.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>


class FontSet {
	public:
		FontSet(
			std::string spec,
			Display* display,  int screen,
			double font_size_override = 0.0, bool skip_italics = false);
		~FontSet();

		XftFont* xft_font_for(const Style& style) {
			return xft_fonts[style.italic << 1 | style.bold];
			}
		XftFont* plain_xft_font() { return xft_fonts[0]; }
		int	ascent() { return xft_fonts[0]->ascent; }
		int height() { return xft_fonts[0]->height; }

		double used_font_size;

	protected:
		Display* display;
		XftFont* xft_fonts[4];
	};



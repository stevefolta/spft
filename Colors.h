#ifndef Colors_h
#define Colors_h

#include <stdint.h>
#include <X11/Xft/Xft.h>
#include <map>

// Colors are represented as uint32_t's.  If the high bit is set, it's a "true
// color" represented as 0x80rrggbb.  Otherwise, it's an ANSI color index.


class Colors {
	public:
		enum {
			true_color_bit = 0x80000000,
			};

		Colors() : initialized(false) {}
		~Colors();

		void	init(Display* display);
		const XftColor*	xft_color(uint32_t color);

	protected:
		bool	initialized;
		XftColor	indexed_colors[256];
		std::map<uint32_t, XftColor>	true_colors;

		Display*	display;
	};

extern Colors colors;


#endif 	// !Colors_h


#include "FontSet.h"
#include <stdexcept>
#include <stdio.h>


FontSet::FontSet(
	std::string spec,
	Display* display_in,  int screen,
	double font_size_override, bool skip_italics)
	: display(display_in)
{
	FcResult result;

	// Regular.
	FcPattern* pattern = FcNameParse((const FcChar8*) spec.c_str());
	if (font_size_override > 0) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, font_size_override);
		}
	XftDefaultSubstitute(display, screen, pattern);
	FcPattern* match = XftFontMatch(display, screen, pattern, &result);
#ifdef SHOW_REAL_FONT
	FcChar8* format_name = FcPatternFormat(match, (FcChar8*) "%{=fcmatch} size: %{size} pixelsize: %{pixelsize}");
	printf("Real font: \"%s\".\n", format_name);
	free(format_name);
#endif
	xft_fonts[0] = XftFontOpenPattern(display, match);
	if (xft_fonts[0] == nullptr)
		throw std::runtime_error("Couldn't open the font.");

	// Get the font size used.
	used_font_size = 0;
	result = FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &used_font_size);

	// Italic.
	// (We do this first because we'll be clobbering the weight later.)
	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	match = XftFontMatch(display, screen, pattern, &result);
	xft_fonts[2] = XftFontOpenPattern(display, match);
	if (xft_fonts[2] == nullptr) {
		fprintf(stderr, "Couldn't open italic font.");
		xft_fonts[2] = xft_fonts[0];
		}

	// Bold italic.
	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	match = XftFontMatch(display, screen, pattern, &result);
	xft_fonts[3] = XftFontOpenPattern(display, match);
	if (xft_fonts[3] == nullptr) {
		fprintf(stderr, "Couldn't open bold italic font.");
		xft_fonts[3] = xft_fonts[0];
		}

	// Bold.
	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	match = XftFontMatch(display, screen, pattern, &result);
	xft_fonts[1] = XftFontOpenPattern(display, match);
	if (xft_fonts[1] == nullptr) {
		fprintf(stderr, "Couldn't open bold font.");
		xft_fonts[1] = xft_fonts[0];
		}

	FcPatternDestroy(pattern);
}


FontSet::~FontSet()
{
	for (int i = 3; i >= 0; --i) {
		bool is_copy = false;
		for (int j = i - 1; j >= 0; --j) {
			if (xft_fonts[i] == xft_fonts[j]) {
				is_copy = true;
				break;
				}
			}
		if (!is_copy)
			XftFontClose(display, xft_fonts[i]);
		xft_fonts[i] = nullptr;
		}
}



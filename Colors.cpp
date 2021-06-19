#include "Colors.h"

Colors colors;


static uint16_t sixd_to_16bit(int x)
{
	// This formula comes from st.
	return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

void Colors::init(Display* display_in)
{
	if (initialized)
		return;

	int i;

	display = display_in;
	int screen = XDefaultScreen(display);
	Visual* visual = XDefaultVisual(display, screen);
	Colormap colormap = XDefaultColormap(display, screen);
	XRenderColor render_color;
	render_color.alpha = 0xFFFF;

	// 16 "themed" colors.
	static uint32_t default_color_theme[] = {
		0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
		0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
		};
	for (i = 0; i < 16; ++i) {
		uint32_t color = default_color_theme[i];
		render_color.red = (color & 0xFF0000) >> 8 | (color & 0xFF0000) >> 16;
		render_color.green = (color & 0xFF00) | (color & 0xFF00) >> 8;
		render_color.blue = (color & 0xFF) << 8 | (color & 0xFF);
		XftColorAllocValue(
			display, visual, colormap,
			&render_color, &indexed_colors[i]);
		}

	// 216 6x6x6 colors.
	static const int greyscale_start = 16 + 6 * 6 * 6;
	for (i = 16; i < greyscale_start; ++i) {
		int color = i - 16;
		render_color.red = sixd_to_16bit((color / 36) % 6);
		render_color.green = sixd_to_16bit((color / 6) % 6);
		render_color.blue = sixd_to_16bit(color % 6);
		XftColorAllocValue(
			display, visual, colormap,
			&render_color, &indexed_colors[i]);
		}

	// 24 greyscale colors.
	for (i = greyscale_start; i < 256; ++i) {
		int greyness = i - greyscale_start;
		render_color.red = 0x0808 + 0x0A0A * greyness;
		render_color.green = render_color.blue = render_color.red;
		XftColorAllocValue(
			display, visual, colormap,
			&render_color, &indexed_colors[i]);
		}

	initialized = true;
}


Colors::~Colors()
{
	if (!initialized)
		return;

	int screen = XDefaultScreen(display);
	Visual* visual = XDefaultVisual(display, screen);
	Colormap colormap = XDefaultColormap(display, screen);

	for (int i = 0; i < 256; ++i)
		XftColorFree(display, visual, colormap, &indexed_colors[i]);
	for (auto& color: true_colors)
		XftColorFree(display, visual, colormap, &color.second);
}


const XftColor* Colors::xft_color(uint32_t color)
{
	if (color < 256)
		return &indexed_colors[color];

	// Otherwise, treat it as true color (even if the true_color_bit isn't set).
	color &= ~true_color_bit;
	if (true_colors.find(color) == true_colors.end()) {
		// Don't have it yet.
		XRenderColor render_color;
		render_color.alpha = 0xFFFF;
		render_color.red = (color & 0xFF0000) >> 8 | (color & 0xFF0000) >> 16;
		render_color.green = (color & 0xFF00) | (color & 0xFF00) >> 8;
		render_color.blue = (color & 0xFF) << 8 | (color & 0xFF);

		int screen = XDefaultScreen(display);
		Visual* visual = XDefaultVisual(display, screen);
		Colormap colormap = XDefaultColormap(display, screen);
		XftColorAllocValue(
			display, visual, colormap,
			&render_color, &true_colors[color]);
		}
	return &true_colors[color];
}




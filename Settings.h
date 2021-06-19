#ifndef Settings_h
#define Settings_h

#include <string>


struct Settings {
	std::string	font_spec;
	std::string	term_name;
	uint32_t	default_foreground_color;
	uint32_t	default_background_color;
	};
extern Settings settings;


#endif 	// !Settings_h



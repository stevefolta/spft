#ifndef Settings_h
#define Settings_h

#include <string>


struct Settings {
	std::string	font_spec;
	std::string	term_name;
	uint32_t	default_foreground_color;
	uint32_t	default_background_color;
	float	estimated_column_width;
		// A guess at the average character width, as a fraction of the width of
		// a capital "M".

	void	read_settings_files();
	void	read_settings_file(std::string path);
	};
extern Settings settings;


#endif 	// !Settings_h



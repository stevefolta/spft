#ifndef Settings_h
#define Settings_h

#include <string>


struct Settings {
	std::string	font_spec;
	std::string	line_drawing_font_spec;
	std::string	term_name;
	uint32_t	default_foreground_color;
	uint32_t	default_background_color;
	float	average_character_width;
		// A guess at the average character width, as a fraction of the width of
		// a capital "M".
	uint32_t double_click_ms;
	std::string word_separator_characters;
	std::string additional_word_separator_characters;
	std::string window_title;
	std::string working_directory;
	uint32_t tab_width, column_separation;
	uint32_t synthetic_tab_spaces;
	std::string geometry;
	uint32_t border;
	bool default_auto_wrap;
	float font_size_increment;

	void	read_settings_files();
	void	read_settings_file(std::string path);
	std::string	home_path();
	};
extern Settings settings;


#endif 	// !Settings_h



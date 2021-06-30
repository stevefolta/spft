#include "Settings.h"
#include <string>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>


Settings settings = {
	.font_spec = "Helvetica Neue,sans\\-serif-16.5:style=Light",
	.term_name = "xterm",
	.default_foreground_color = 0,
	.default_background_color = 15,
	.average_character_width = 0.6,
	.double_click_ms = 300,
	.word_separator_characters = " \t!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}",
	.additional_word_separator_characters = "",
	.window_title = "spft",
	.indent_width = 30,
	.column_separation = 20,
	};


class parse_error : public std::runtime_error {
	public:
		parse_error(const std::string& message)
			: std::runtime_error(message) {}
	};


class SettingsParser {
	public:
		SettingsParser(const char* text, int length)
			: p(text), end(text + length) {}

		void	parse();

	protected:
		const char*	p;
		const char*	end;

		std::string	next_token();
		bool	is_identifier(std::string token) { return isalpha(token[0]); }
		std::string	unquote_string(std::string token);
		uint32_t	parse_uint32(std::string token);
		float	parse_float(std::string token);
	};


void SettingsParser::parse()
{
	try {
		while (true) {
			std::string token = next_token();
			if (token.empty())
				break;
			if (token == "," || token == ";") {
				// We allow commas and semicolons between settings.
				continue;
				}

			// Settings have the form "<name> = <value>".
			if (!is_identifier(token)) {
				std::stringstream message;
				message << "Not a setting name: \"" << token << '"';
				throw parse_error(message.str());
				}
			std::string setting_name = token;
			if (next_token() != "=") {
				std::stringstream message;
				message << "Missing '=' for \"" << setting_name << "\" setting.";
				throw parse_error(message.str());
				}
			std::string value_token = next_token();
			if (value_token.empty()) {
				std::stringstream message;
				message << "Missing value for \"" << setting_name << "\" setting.";
				throw parse_error(message.str());
				}

			// Set the setting.
			try {
				if (setting_name == "font")
					settings.font_spec = unquote_string(value_token);
				else if (setting_name == "term_name")
					settings.term_name = unquote_string(value_token);
				else if (setting_name == "default_foreground_color")
					settings.default_foreground_color = parse_uint32(value_token);
				else if (setting_name == "default_background_color")
					settings.default_background_color = parse_uint32(value_token);
				else if (setting_name == "average_character_width")
					settings.average_character_width = parse_float(value_token);
				else if (setting_name == "double_click_ms")
					settings.double_click_ms = parse_uint32(value_token);
				else if (setting_name == "word_separator_characters")
					settings.word_separator_characters = unquote_string(value_token);
				else if (setting_name == "additional_word_separator_characters")
					settings.additional_word_separator_characters = unquote_string(value_token);
				else if (setting_name == "window_title")
					settings.window_title = unquote_string(value_token);
				else if (setting_name == "indent_width")
					settings.indent_width = parse_uint32(value_token);
				else if (setting_name == "column_separation")
					settings.column_separation = parse_uint32(value_token);
				else
					fprintf(stderr, "Unknown setting: %s.\n", setting_name.c_str());
				}
			catch (parse_error& e) {
				fprintf(stderr, "Invalid \"%s\" setting: %s\n", setting_name.c_str(), e.what());
				}
			}
		}
	catch (parse_error& e) {
		fprintf(stderr, "Error in settings file: %s\n", e.what());
		}
}


std::string SettingsParser::next_token()
{
	char c;

	// Skip whitespace and comments.
	while (true) {
		if (p >= end)
			return "";
		c = *p;
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			p += 1;
		else if (c == '#') {
			// Skip to end of line.
			p += 1;
			while (true) {
				if (p >= end)
					return "";
				c = *p++;
				if (c == '\r' || c == '\n')
					break;
				}
			}
		else
			break;
		}

	const char* token_start = p;
	c = *p++;

	if (c == '"' || c == '\'') {
		// String.
		char quote_char = c;
		while (true) {
			if (p >= end)
				throw parse_error("Unterminated string.");
			c = *p++;
			if (c == quote_char)
				break;
			if (c == '\\') {
				// Consume the next character.
				if (p >= end)
					throw parse_error("Unterminated string.");
				p += 1;
				}
			}
		}

	else if (isalpha(c)) {
		// Identifier.
		while (p < end) {
			c = *p;
			if (!isalnum(c) && c != '_')
				break;
			p += 1;
			}
		}

	else if (isdigit(c)) {
		// Number.
		if (c == '0' && p < end && (*p == 'x' || *p == 'X')) {
			// Hex number.
			p += 1; 	// Skip the "x".
			if (p >= end || !isxdigit(*p))
				throw parse_error("Incomplete hex number.");
			while (p < end) {
				if (!isxdigit(*p))
					break;
				p += 1;
				}
			}
		else {
			// Decimal number (potentially a float).
			while (p < end) {
				c = *p;
				if (!isdigit(c) && c != '.')
					break;
				p += 1;
				}
			}
		}

	else if (c == '=' || c == ',' || c == ';') {
		// These are single-character tokens.
		}

	else {
		// Unknown.
		std::stringstream message;
		message << "Invalid character: '" << c << "'";
		throw parse_error(message.str());
		}

	return std::string(token_start, p  - token_start);
}


std::string SettingsParser::unquote_string(std::string token)
{
	if (token.empty() || (token[0] != '"' && token[0] != '\''))
		throw parse_error("Not a string: " + token);

	token = token.substr(1, token.length() - 2);
	std::stringstream result;
	for (auto p = token.begin(); p < token.end(); ++p) {
		char c = *p;
		if (c == '\\')
			p += 1;
		result << c;
		}
	return result.str();
}


uint32_t SettingsParser::parse_uint32(std::string token)
{
	char* end_ptr = nullptr;
	uint32_t result = strtoul(token.c_str(), &end_ptr, 0);
	if (*end_ptr != 0)
		throw parse_error("Not a number: " + token);
	return result;
}


float SettingsParser::parse_float(std::string token)
{
	char* end_ptr = nullptr;
	float result = strtof(token.c_str(), &end_ptr);
	if (*end_ptr != 0)
		throw parse_error("Not a floating-point number: " + token);
	return result;
}



void Settings::read_settings_files()
{
	// $XDG_CONFIG_HOME/spft/settings is actually the only settings file we
	// read.

	std::string config_dir;
	const char* env_dir = getenv("XDG_CONFIG_HOME");
	if (env_dir)
		config_dir = env_dir;
	else {
		// Default to ~/.config.
		std::string home_dir = home_path();
		if (!home_dir.empty())
			config_dir = home_dir + "/.config";
		}
	if (!config_dir.empty())
		read_settings_file(config_dir + "/spft/settings");
}


void Settings::read_settings_file(std::string path)
{
	std::fstream file(path);
	if (!file.is_open()) {
		// All settings files are optional, so don't complain if it doesn't
		// exist.
		return;
		}
	std::string contents(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	SettingsParser parser(contents.data(), contents.size());
	parser.parse();
}


std::string Settings::home_path()
{
	const char* home_dir = getenv("HOME");
	if (home_dir == nullptr) {
		const struct passwd* user_info = getpwuid(getuid());
		if (user_info && user_info->pw_dir[0])
			home_dir = user_info->pw_dir;
		}
	if (home_dir == nullptr)
		return "";
	return home_dir;
}




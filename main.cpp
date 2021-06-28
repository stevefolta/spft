#include "TermWindow.h"
#include "Settings.h"
#include <string>
#include <list>
#include <stdio.h>


static const char* prog_name = "spft";
static int usage()
{
	fprintf(stderr, "Usage: %s [-T <title> | title: <title>] [working-directory: <dir>]\n", prog_name);
	fprintf(stderr, "  Keyword-style arguments also accept the traditional double-dash syntax\n");
	fprintf(stderr, "  (eg. \"--title\" instead of \"title:\").\n");
	return 1;
}


int main(int argc, char* argv[])
{
	// Read settings.
	settings.read_settings_files();

	// Read the arguments.
	prog_name = argv[0];
	std::list<std::string> args(&argv[1], &argv[argc]);
	while (!args.empty()) {
		std::string arg = args.front();
		args.pop_front();
		if (arg.substr(0, 2) == "--")
			arg = arg.substr(2) + ":";
		if (arg == "-T" || arg == "title:") {
			if (args.empty())
				return usage();
			settings.window_title = args.front();
			args.pop_front();
			}
		else if (arg == "working-directory:") {
			if (args.empty())
				return usage();
			settings.working_directory = args.front();
			args.pop_front();
			}
		else
			return usage();
		}

	// Create the window.
	TermWindow* window = new TermWindow();
	while (!window->is_done())
		window->tick();
	delete window;
	return 0;
}


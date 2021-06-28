#include "TermWindow.h"
#include "Settings.h"
#include <stdio.h>
#include <string>
#include <list>


static const char* prog_name = "spft";
static int usage()
{
	fprintf(stderr, "Usage: %s [-T <title> | title: <title>]\n", prog_name);
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
		if (arg == "-T" || arg == "title:") {
			if (args.empty())
				return usage();
			settings.window_title = args.front();
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


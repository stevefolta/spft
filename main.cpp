#include "TermWindow.h"
#include "Settings.h"


int main(int argc, char* argv[])
{
	settings.read_settings_files();

	TermWindow* window = new TermWindow();
	while (!window->is_done())
		window->tick();
	delete window;
	return 0;
}


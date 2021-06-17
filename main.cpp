#include "TermWindow.h"


int main(int argc, char* argv[])
{
	TermWindow* window = new TermWindow();
	while (!window->is_done())
		window->tick();
	delete window;
	return 0;
}


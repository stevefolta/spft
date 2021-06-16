#include "TermWindow.h"
#include "Terminal.h"
#include "History.h"
#include <X11/Xft/Xft.h>
#include <stdexcept>


TermWindow::TermWindow()
{
	terminal = new Terminal();
	history = new History();

	display = XOpenDisplay(nullptr);
	if (display == nullptr)
		throw std::runtime_error("Can't open display");
	screen = XDefaultScreen(display);

	//*** TODO
}


TermWindow::~TermWindow()
{
	delete terminal;
	delete history;
}




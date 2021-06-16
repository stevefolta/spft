#ifndef TermWindow_h
#define TermWindow_h

// This would just be called "Window", but that conflicts with XWindow's type
// of the same name.

#include <X11/Xlib.h>

class Terminal;
class History;


class TermWindow {
	public:
		TermWindow();
		~TermWindow();

	protected:
		Terminal* terminal;
		History* history;
		Display*	display;
		int screen;
		Window window;
	};


#endif 	// !TermWindow_h


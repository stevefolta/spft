spft
=====

Simple Proportional-Font Terminal

Inspired by mlterm and st.

mlterm is the only X terminal program I've found that properly supports
proportional (that is, non-monospace) fonts.  There should be more than one...
and now there is.  Also, they broke some of the behavior around the
Shift-PageUp/Down keys in version 3.9.1.  It seems they wanted to make
scrolling a "mode" now, which it hadn't been before, and they slipped this
major UI change and its associated breakage into a point-point release.  That
ticked me off enough to prompt me to write this.

st is wonderfully simple in how it's coded.  It's a lot easier to find your way
around its code than it is for mlterm.  It provided accessible examples of how
to do various things.

spft is somewhere between the two in both functionality and code style.  spft
isn't written in a "suckless" style like st is (it's in C++, for a start), but
it still aspires to be as minimal and direct as possible.  It compiles in under
a second for me if I use all the cores on my machine.


Status
-----

It's pretty usable at this point, but perhaps not ready to take over from
mlterm as your daily driver.


Elastic Tabs
-----

spft is the first terminal program I know of that supports elastic tabs.  In
fact, all tabs are elastic tabs currrently.  The problem is that no existing
programs output tab characters in an appropriate manner for elastic tabs, so
things are often displayed badly.  (Often a wide column on one line will be an
empty "column" on another line, pushing the meat of the second line way to the
right.)  The solution will be to add new escape sequences so programs that
support elastic tab-compatible output can enable elastic tabs (and disable them
when done).  This will also allow them to indicate which lines should have
their columns grouped together.

An "els" script is included; it wraps "ls" so the output is appropriate for
elastic tabs.

Column widths are currently calculated "on-the-fly" while drawing.  This means
they are not "stable": columns can shift around as you scroll.  Having an
escape sequence to enable elastic tabs would also make it a little easier to
make the column widths stable.


Recommended Fonts
-----

I use "Helvetica Neue Light".  I wish I could find a free font that looks as nice as that.


What it still needs
-----

- Elastic tabs only when requested, regular tabs otherwise.
- Many more escape sequences.
- More testing of character insertion and deletion scenarios?
- Handle requests for big selections correctly.
- Settable window class and other WM hints.



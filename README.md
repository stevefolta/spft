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
cheesed me off enough to prompt me to write this.

st is wonderfully simple in how it's coded.  It's a lot easier to find your way
around its code than it is for mlterm.  It provided accessible examples of how
to do various things.

spft is somewhere between the two in both functionality and code style.  spft
isn't written in a "suckless" style like st is (it's in C++, for a start), but
it still aspires to be as minimal and direct as possible.


Status
-----

It's pretty usable at this point, but probably not ready to take over from
mlterm as your daily driver.


What it still needs
-----

- Bold and italic.
- Tabs.  Elastic tabs.
- Many more escape sequences.
- More testing of character insertion and deletion scenarios.
- Handle requests for big selections correctly.
- Settable window class and other WM hints.



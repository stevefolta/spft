spft
=====

Simple Proportional-Font Terminal

Inspired by mlterm and st.

mlterm is the only X terminal program I've found that properly supports
proportional (that is, non-monospace) fonts.  There should be more than one...
Also, they broke some of the behavior around the Shift-PageUp/Down keys in
version 3.9.1.

st is wonderfully simple in how it's coded.  spft isn't written in a "suckless"
style (it's in C++, for a start), but it aspires to be similarly minimal and
direct.


Status
-----

It's pretty usable at this point, but probably not ready to take over from
mlterm as your daily driver.


What it still needs
-----

- Selection.
- Pasting.
- Bold and italic.
- Tabs.  Elastic tabs.
- Many more escape sequences.
- More testing of character insertion and deletion scenarios.



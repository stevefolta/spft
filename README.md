spft
=====

Simple Proportional-Font Terminal

Inspired by mlterm and st.

mlterm was the only X terminal program I'd found that properly supports
proportional (that is, non-monospace) fonts.  There should be more than one...
and now there is.  Also, they broke some of the behavior around the
Shift-PageUp/Down keys in version 3.9.1.  It seems they wanted to make
scrolling a "mode" now, which it hadn't really been before, and they slipped
this significant UI change and its associated breakage into a point-point
release.  That ticked me off enough to prompt me to write this.

st is wonderfully simple in how it's coded.  It's a lot easier to find your way
around its code than it is for mlterm.  It provided accessible examples of how
to do various things.

spft is somewhere between the two in both functionality and code style.  spft
isn't written in a "suckless" style like st is (it's in C++, for a start), but
it still aspires to be as minimal and direct as possible.  It compiles in under
a second for me if I use all the cores on my machine.  It also appears to use
much less memory than mlterm, even when keeping much more scrollback history.


Status
-----

It doesn't handle every escape sequence known to man, but it seems quite usable
at this point.


Elastic Tabs
-----

spft is the first terminal program I know of that supports elastic tabs.  At
first, I had it do elastic tabs for all tabs.  But elastic tabs look terrible
if applied to output that's generated for tab-stop tabs.  (Often a wide column
on one line will be an empty "column" on another line, pushing the meat of the
second line way to the right.)  

So elastic tabs are enabled by a couple of special escape sequences.
"\x1B[?5001h" starts a group of elastic-tabbed lines with the line that the
cursor is on.  "\x1B[?5001l" ends that group; the line containing the cursor is
not part of it.

An "els" script is included; it wraps "ls" so the output is appropriate for
elastic tabs.  You might want to put something like this in your .bashrc:

```bash
if [ -n "$SPFT" ]; then
    alias ls='els --color=always'
    fi
```


Recommended Fonts
-----

I use "Helvetica Neue Light".  I wish I could find a free font that looks as nice as that.


What it still needs
-----

- More escape sequences.
- Handle requests for big selections correctly.
- Settable window class and other WM hints.



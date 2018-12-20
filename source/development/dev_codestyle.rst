rsyslog code style
==================

**Note**: code style is still under construction. This guide lists
some basic style requirements.

**Code that does not match the code style guide will not pass CI testing.**

The following is required right now:

  * we use ANSI C99
  * indentation is done with tabs, not spaces
  * trailing whitespace in lines is not permitted
  * lines longer than 120 characters are not permitted;
    everything over 120 chars is rejected and must be reformatted.

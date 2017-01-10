rsylog code style
=================

**Note**: code style is still under construction. This guide lists
some basic style requirements.

**Code that does not match the code style guide will not pass CI testing.**

The following is required right now:

  * we use ANSCI C99
  * indention is done with tabs, not spaces
  * trailing whitespace in lines is not permitted
  * lines longer than 140 characters are not permitted; also, it is
    advised to keep lines shorter than 120 characters unless there is
    a good reason. The 120 char rule is not enforced, but everything
    over 140 chars is rejected and must be reformatted.

mmrm1stspace: First Space Modification Module
=============================================

**Author:** Pascal Withopf <pascalwithopf1@gmail.com>

In rfc3164 the msg begins at the first letter after the tag. It is often the
case that this is a unnecessary space. This module removes this first character
if it is a space.

Configuration Parameters
------------------------

Note: parameter names are case-insensitive.

Currently none.

Examples
--------

This example receives messages over imtcp and modifies them, before sending
them to a file.

::

   module(load="imtcp")
   module(load="mmrm1stspace")
   input(type="imtcp" port="13514")
   action(type="mmrm1stspace")
   action(type="omfile" file="output.log")


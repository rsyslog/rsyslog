************
re_match_i()
************

Purpose
=======

re_match_i(expr, re)

Returns 1, if expr matches re, 0 otherwise. Uses POSIX ERE. In contrast to
`re_match()` the matching is case-insensitive.

.. note::

   Functions using regular expressions tend to be slow and other options
   may be faster.


Example
=======

In the following example it is checked if the msg object matches the regex string.

.. code-block:: none

   re_match($msg,'TesT')

It matches it the message "Test", in any case ("TEST", "tEst", ...)
is contained in msg property.

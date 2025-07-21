**********
re_match()
**********

Purpose
=======

re_match(expr, re)

Returns 1, if expr matches re, 0 otherwise. Uses POSIX ERE. In contrast to
`re_match_i()` the matching is case-sensitive.

.. note::

   Functions using regular expressions tend to be slow and other options
   may be faster.


Example
=======

In the following example it is checked if the msg object matches the regex string.

.. code-block:: none

   re_match($msg,'(5[1-5][0-9]{14})')



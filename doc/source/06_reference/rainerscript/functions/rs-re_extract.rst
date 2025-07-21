************
re_extract()
************

Purpose
=======

re_extract(expr, re, match, submatch, no-found)

Extracts data from a string (property) via a regular expression match.
POSIX ERE regular expressions are used. The variable "match" contains
the number of the match to use. This permits to pick up more than the
first expression match. Submatch is the submatch to match (max 50 supported).
The "no-found" parameter specifies which string is to be returned in case
when the regular expression is not found. Note that match and
submatch start with zero. It currently is not possible to extract
more than one submatch with a single call.

This function performs case-sensitive matching. Use the otherwise-equivalent
:doc:`re_extract_i <rs-re_extract_i>` function to perform case-insensitive
matches.

.. note::

   Functions using regular expressions tend to be slow and other options
   may be faster.


Example
=======

In the following example the msg object is checked for the regex string.
Only the first match is used and if no match was found an empty string is returned.

.. code-block:: none

   re_extract($msg,'(5[1-5][0-9]{14})',0,1,"")


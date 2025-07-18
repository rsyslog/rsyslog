*******
field()
*******

Purpose
=======

field(str, delim, matchnbr)

Returns a field-based substring. str is
the string to search, delim is the delimiter and matchnbr is the
match to search for (the first match starts at 1). This works similar
as the field based property-replacer option. Versions prior to 7.3.7
only support a single character as delimiter character. Starting with
version 7.3.7, a full string can be used as delimiter. If a single
character is being used as delimiter, delim is the numerical ascii
value of the field delimiter character (so that non-printable
characters can by specified). If a string is used as delimiter, a
multi-character string (e.g. "#011") is to be specified.

.. note::

   When a single character is specified as string
   ``field($msg, ",", 3)`` a string-based extraction is done, which is
   more performance intensive than the equivalent single-character
   ``field($msg, 44 ,3)`` extraction.


Example
=======

With ascii value of the field delimiter
---------------------------------------

Following example returns the third field delimited by space.
In this example $msg will be "field1 field2 field3 field4".

.. code-block:: none

   set $!usr!field = field($msg, 32, 3);

produces

.. code-block:: none

   "field3"


With a string as the field delimiter
------------------------------------

This example returns the second field delimited by "#011".
In this example $msg will be "field1#011field2#011field3#011field4".

.. code-block:: none

   set $!usr!field = field($msg, "#011", 2); -- the second field, delimited by "#011"

produces

.. code-block:: none

   "field2"


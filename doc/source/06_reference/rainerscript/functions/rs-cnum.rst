******
cnum()
******

Purpose
=======

cnum(expr)

Converts expr to a number (integer).

.. note::

   If the expression does not contain a numerical value, the following
   rule applies: the best match as the number is returned. For example
   "1x234" will return the number 1 and "Test123" will return 0. Zero is
   always returned if the there is no number at the start of the string.
   This also is the case for empty strings.


Example
=======



.. code-block:: none

   cnum(3+2);

produces

.. code-block:: none

   5



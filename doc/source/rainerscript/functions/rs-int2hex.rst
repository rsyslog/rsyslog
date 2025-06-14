*********
int2hex()
*********

Purpose
=======

int2hex(num)

Returns a hexadecimal number string of a given positive integer num.

.. note::

   When a negative integer is given the return value will be the
   `Two's complement <https://en.wikipedia.org/wiki/Two%27s_complement>`_ of
   the input number.


Example
=======


Positive Integer given
----------------------

In the following example the hexadecimal number of 61 is returned
as a string.

.. code-block:: none

   int2hex(61)

produces

.. code-block:: none

   3d


Negative Integer given
----------------------

This example shows the result on a 64-bit system when a negative
integer such as -13 is put into the function.

.. code-block:: none

   int2hex(-13)

produces

.. code-block:: none

   fffffffffffffff3



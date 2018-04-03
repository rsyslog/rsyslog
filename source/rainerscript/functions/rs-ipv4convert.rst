***********************
num2ipv4() / ipv42num()
***********************

Purpose
=======

num2ipv4
--------

num2ipv4(int)

Converts an integer into an IPv4-address and returns the address as string.
Input is an integer with a value between 0 and 4294967295. The output format
is '>decimal<.>decimal<.>decimal<.>decimal<' and '-1' if the integer input is
invalid or if the function encounters a problem.


ipv42num
--------

ipv42num(str)

Converts an IPv4-address into an integer and returns the integer. Input is
a string; the expected address format may include spaces in the beginning
and end, but must not contain any other characters in between (except dots).
If the format does include these, the function results in an error and returns -1.


Example
=======

num2ipv4
--------

This example shows an integer being converted to an ipv4 address.

.. code-block:: none

   num2ipv4(123456)


ipv42num
--------

This example shows the parameter fromhost-ip being converted to an integer.

.. code-block:: none

   ipv42num($fromhost-ip)



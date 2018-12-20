***********
substring()
***********

Purpose
=======

substring(str, start, subStringLength)

Creates a substring from str. The substring begins at start and is
at most subStringLength characters long.

.. note::

   The first character of the string has the value 0. So if you set
   start to '2', the substring will start with the third character.


Example
=======

In the following example a substring from the string is created,
starting with the 4th character and being 5 characters long.

.. code-block:: none

   substring("123456789", 3, 5)


This example will result in the substring "45678".

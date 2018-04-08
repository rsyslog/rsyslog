***********
substring()
***********

Purpose
=======

substring(str, start, subStringLength)

Creates a substring from str. The substring begins at start and is
at most subStringLength characters long.

.. note::

   The first character of the string has the value 1. So if you set
   start to '3', the substring will start with the third character.


Example
=======

In the following example a substring from the message property is created,
starting with the 10th character and being 5 characters long.

.. code-block:: none

   substring($msg, 10, 5)



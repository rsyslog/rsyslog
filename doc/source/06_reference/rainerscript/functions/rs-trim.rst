*****************
ltrim() / rtrim()
*****************

Purpose
=======

ltrim
-----

ltrim(str)

Removes any spaces at the start of a given string. Input is a string, output
is the same string starting with the first non-space character.


rtrim
-----

rtrim(str)

Removes any spaces at the end of a given string. Input is a string, output
is the same string ending with the last non-space character.


Example
=======

ltrim
-----

In the following example the spaces at the beginning of the string are removed.

.. code-block:: none

   ltrim("   str ")

produces

.. code-block:: none

   "str "


rtrim
-----

In the following example the spaces at the end of the string are removed.

.. code-block:: none

   rtrim("   str ")

produces

.. code-block:: none

   "   str"


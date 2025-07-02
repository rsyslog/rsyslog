******
wrap()
******

Purpose
=======

wrap(str, wrapper_str)
wrap(str, wrapper_str, escaper_str)

Returns the str wrapped with wrapper_str.
In the second syntax additionally, any instances of wrapper_str appearing
in str would be replaced by the escaper_str.


Example
=======

Syntax 1
--------

The following example shows how the string is wrapped into
the wrapper_string.

.. code-block:: none

   wrap("foo bar", "##")

produces

.. code-block:: none

   "##foo bar##"


Syntax 2
--------

This example shows how parts of the string are being replaced.

.. code-block:: none

   wrap("foo'bar", "'", "_")

produces

.. code-block:: none

   "'foo_bar'"



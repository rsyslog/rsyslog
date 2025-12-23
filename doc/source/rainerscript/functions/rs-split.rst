split()
=======

Purpose
-------

Splits a string into a JSON array using a specified separator.

Syntax
------

.. code-block:: none

   split(string, separator)

Parameters
----------

string
   The input string to be split.

separator
   The delimiter string used to split the input. Can be a single character
   or a multi-character string.

Return Value
------------

Returns a JSON array containing the split substrings.

Examples
--------

.. code-block:: none

   # Single-character separator
   set $!tags = "error,warning,info";
   set $!tag_array = split($!tags, ",");
   # Result: ["error", "warning", "info"]

   # Multi-character separator
   set $!data = "item1::item2::item3";
   set $!items = split($!data, "::");
   # Result: ["item1", "item2", "item3"]

See Also
--------

- :doc:`field() <rs-field>` - Extract a single field from delimited data
- :doc:`parse_json() <rs-parse_json>` - Parse a JSON string
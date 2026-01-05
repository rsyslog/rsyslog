append_json()
=============

Purpose
-------

Appends an element to a JSON array or adds a key-value pair to a JSON object,
returning a new object (the original is not modified).

Syntax
------

.. code-block:: none

   append_json(array, element)
   append_json(object, key, value)

Parameters
----------

For arrays (2 parameters):

array
   The input JSON array to append to.

element
   The element to add at the end of the array. Can be a string, number,
   or JSON value.

For objects (3 parameters):

object
   The input JSON object to add a key-value pair to.

key
   The key name (string) for the new property.

value
   The value to associate with the key. Can be a string, number,
   or JSON value.

Return Value
------------

Returns a new JSON array or object with the element/property added.
The original input is not modified. Returns null if the input is invalid.

Examples
--------

.. code-block:: none

   # Append to an array
   set $.ret = parse_json('["error", "warning"]', "$!tags");
   set $!tags = append_json($!tags, "info");
   # Result: ["error", "warning", "info"]

   # Add a property to an object (assign to a new variable)
   set $.ret = parse_json('{"name": "test"}', "$!data");
   set $!result = append_json($!data, "status", "active");
   # Result in $!result: {"name": "test", "status": "active"}

   # Chain multiple appends on arrays
   set $.ret = parse_json('[]', "$!arr");
   set $!arr = append_json($!arr, "first");
   set $!arr = append_json($!arr, "second");
   # Result: ["first", "second"]

See Also
--------

- :doc:`split() <rs-split>` - Split a string into a JSON array
- :doc:`parse_json() <rs-parse_json>` - Parse a JSON string

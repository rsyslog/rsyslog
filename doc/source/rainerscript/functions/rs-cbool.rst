*******
cbool()
*******

Purpose
-------

cbool(expr)

Converts expr to a numeric boolean value. The result is ``0`` for false and
``1`` for true.

String input is trimmed before conversion. Empty strings, ``0``, ``false``,
``off``, and ``no`` convert to ``0``. Other non-empty strings convert to ``1``.
String matching is case-insensitive.

This is useful when a value should later be rendered as a JSON boolean with a
list template property that uses ``format="jsonf"`` and ``datatype="bool"``.

Example
-------

.. code-block:: none

   set $!enabled = cbool("false");

   template(name="out" type="list" option.jsonf="on") {
     property(outname="enabled" name="$!enabled" format="jsonf" datatype="bool")
   }

produces

.. code-block:: json

   {"enabled":false}

.. _ref-templates-statement-constant:

Constant statement
==================

.. summary-start

Outputs literal text. Supports escape sequences and optional JSON field
formatting when an outname and format are specified.
.. summary-end

This statement specifies constant text. It is primarily intended for
text-based output so constant fragments, such as newlines, can be
included. Example from the testbench:

::

    template(name="outfmt" type="list") {
        property(name="$!usr!msgnum")
        constant(value="\n")
    }

The following escape sequences are recognized inside the constant text:

- ``\\`` – single backslash
- ``\n`` – LF
- ``\ooo`` – three octal digits representing a character (``\101`` is
  ``"A"``). Exactly three digits must be given.
- ``\xhh`` – two hexadecimal digits representing a character
  (``\x41`` is ``"A"``). Exactly two digits must be given.

If an unsupported character follows a backslash, behavior is
unpredictable.

When creating structured outputs, constant text without an ``outname``
parameter is ignored. To include constant data in the name/value tree,
provide ``outname`` as shown:

.. code-block:: none

    template(name="outfmt" type="list") {
        property(name="$!usr!msgnum")
        constant(value="\n" outname="IWantThisInMyDB")
    }

To generate a constant JSON field the ``format`` parameter can be used:

.. code-block:: none

   template(name="outfmt" type="list" option.jsonf="on") {
             property(outname="message" name="msg" format="jsonf")
             constant(outname="@version" value="1" format="jsonf")
   }

Here the constant statement generates ``"@version":"1"``. Both ``value``
and ``format`` must be specified.

When you want dotted ``outname`` values to be emitted as nested objects,
enable ``option.jsonftree="on"`` on the template instead of ``option.jsonf``.

Parameters
----------

- ``value`` – constant value to emit
- ``outname`` – output field name for structured outputs
- ``format`` – empty or ``jsonf``

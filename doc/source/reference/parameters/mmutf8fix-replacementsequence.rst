.. _param-mmutf8fix-replacementsequence:
.. _mmutf8fix.parameter.input.replacementsequence:

replacementSequence
===================

.. index::
   single: mmutf8fix; replacementSequence
   single: replacementSequence

.. summary-start

Defines a byte sequence used to substitute invalid input bytes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmutf8fix`.

:Name: replacementSequence
:Scope: input
:Type: string
:Default: not set
:Required?: no
:Introduced: 8.2606.0

Description
-----------
This parameter configures a replacement byte sequence for each invalid
input byte detected by mmutf8fix. It is useful when a single printable
US-ASCII character is not enough, for example when operators want to use
the UTF-8 encoding of ``U+FFFD`` as the invalid-character marker.

``replacementSequence`` and :ref:`param-mmutf8fix-replacementchar` are
mutually exclusive. Use ``replacementChar`` when a single-byte
replacement is sufficient. Use ``replacementSequence`` only when a
multi-byte marker is needed; it is an explicit opt-in to the rebuild path
because the output field can become longer than the original input.

Each invalid input byte is replaced independently. For example, if an
invalid two-byte sequence is detected and ``replacementSequence`` is set
to the UTF-8 bytes for ``U+FFFD``, the output contains two replacement
markers.

Use byte escapes in object-parameter strings. For example, write the UTF-8
bytes for ``U+FFFD`` as ``\xef\xbf\xbd``. The equivalent octal spelling is
``\357\277\275``. Hexadecimal escapes use exactly two digits, and octal
escapes use exactly three digits.

Input usage
-----------
.. _mmutf8fix.parameter.input.replacementsequence-usage:

.. code-block:: rsyslog

   module(load="mmutf8fix")

   # U+FFFD encoded as UTF-8 bytes.
   action(type="mmutf8fix" replacementSequence="\xef\xbf\xbd")

YAML usage
----------

.. code-block:: yaml

   modules:
     - load: "mmutf8fix"

   rulesets:
     - name: main
       script: |
         action(type="mmutf8fix" replacementSequence="\xef\xbf\xbd")

See also
--------
See also :doc:`../../configuration/modules/mmutf8fix`.

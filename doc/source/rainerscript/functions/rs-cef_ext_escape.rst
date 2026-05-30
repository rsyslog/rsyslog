cef_ext_escape()
================

Purpose
-------

Escapes a string for safe use as a CEF extension field value.

Syntax
------

.. code-block:: none

   cef_ext_escape(value)

Parameters
----------

value
   The string to escape.

Return Value
------------

Returns the escaped string with the following substitutions applied per
the CEF spec:

- Backslash ``\`` becomes ``\\``
- Equals sign ``=`` becomes ``\=``
- Newline becomes ``\n``
- Carriage return becomes ``\r``

Use this function when embedding dynamic rsyslog property values inside
the extensions argument of :doc:`tocef() <rs-tocef>`. The extensions
argument itself is appended verbatim by ``tocef()``, so any ``=`` or
``\`` in a value must be escaped before concatenation.

Examples
--------

.. code-block:: none

   # Escape a file path containing backslashes
   set $!ext = cef_ext_escape("C:\\dir\\file.txt");
   # Result: C:\\dir\\file.txt

   # Escape a value containing an equals sign
   set $!ext = cef_ext_escape("status=ok");
   # Result: status\=ok

   # Use inside tocef()
   set $!cef = tocef("0", "MyVendor", "rsyslog", "1.0",
                     $syslogtag, $msg, "5",
                     "filePath=" & cef_ext_escape($!path) &
                     " msg="     & cef_ext_escape($msg));

See Also
--------

- :doc:`tocef() <rs-tocef>` - Build a complete CEF header string

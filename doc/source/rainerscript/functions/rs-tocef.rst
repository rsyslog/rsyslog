tocef()
*******

Purpose
-------

Builds a CEF (Common Event Format) header string from the seven mandatory
pipe-delimited header fields and an extensions string.

Syntax
------

.. code-block:: none

   tocef(version, vendor, product, devversion, eventclassid, name, severity, extensions)

Parameters
----------

version
   CEF version number. Use ``"0"`` for CEF 0.x or ``"1"`` for CEF 1.x.

vendor
   Device vendor string.

product
   Device product string.

devversion
   Device version string.

eventclassid
   Unique identifier for the event type. This field is escaped like the
   other header fields, and additionally escapes ``=``, ``%``, and ``#`` as
   ``\=``, ``\%``, and ``\#``.

name
   Human-readable description of the event.

severity
   Event severity. Valid string values: ``Unknown``, ``Low``, ``Medium``,
   ``High``, ``Very-High``. Valid integer values: ``0``-``10``.

extensions
   Pre-formed key=value extension pairs separated by spaces. Appended
   verbatim. Use :doc:`cef_ext_escape() <rs-cef_ext_escape>` to escape
   dynamic property values before embedding them here.

Return Value
------------

Returns a string containing the complete CEF line:
``CEF:version|vendor|product|devversion|eventclassid|name|severity|extensions``

The seven header fields are automatically escaped per the CEF spec:
backslash becomes ``\\`` and pipe becomes ``\|``.

Examples
--------

.. code-block:: none

   set $!cef = tocef("0", "MyVendor", "rsyslog", "1.0",
                     $syslogtag, $msg, "5",
                     "src=" & $fromhost-ip & " spt=514");

   # With extension value escaping for dynamic fields
   set $!cef = tocef("0", "MyVendor", "rsyslog", "1.0",
                     $syslogtag, $msg, "5",
                     "src=" & $fromhost-ip &
                     " msg=" & cef_ext_escape($msg));

See Also
--------

- :doc:`cef_ext_escape() <rs-cef_ext_escape>` - Escape a CEF extension field value

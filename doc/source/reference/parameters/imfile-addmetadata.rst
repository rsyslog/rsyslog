.. _param-imfile-addmetadata:
.. _imfile.parameter.module.addmetadata:

addMetadata
===========

.. index::
   single: imfile; addMetadata
   single: addMetadata

.. summary-start

**Default: see intro section on Metadata**  This is used to turn on or off the addition of metadata to the message object.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: addMetadata
:Scope: input
:Type: boolean
:Default: input=-1
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
**Default: see intro section on Metadata**

This is used to turn on or off the addition of metadata to the
message object.

Input usage
-----------
.. _param-imfile-input-addmetadata:
.. _imfile.parameter.input.addmetadata:
.. code-block:: rsyslog

   input(type="imfile" addMetadata="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

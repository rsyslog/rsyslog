.. _param-imfile-addmetadata:
.. _imfile.parameter.module.addmetadata:

addMetadata
===========

.. index::
   single: imfile; addMetadata
   single: addMetadata

.. summary-start

Controls whether file-related metadata is added to messages. By default, this is automatically enabled when wildcards are used in the file path.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: addMetadata
:Scope: input
:Type: word
:Default: input=auto (enabled for wildcards, disabled otherwise)
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This parameter controls whether file-related metadata is attached to each
message. If left unspecified, metadata is automatically added when wildcards
appear in the ``File`` setting; otherwise it is disabled.

Input usage
-----------
.. _param-imfile-input-addmetadata:
.. _imfile.parameter.input.addmetadata:
.. code-block:: rsyslog

   input(type="imfile" addMetadata="...")

Notes
-----
- Allowed values: ``on`` or ``off``. If omitted, the value is ``auto``.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

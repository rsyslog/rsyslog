.. _param-imfile-addmetadata:
.. _imfile.parameter.input.addmetadata:
.. _imfile.parameter.addmetadata:

addMetadata
===========

.. index::
   single: imfile; addMetadata
   single: addMetadata

.. summary-start

Turns metadata addition on or off; enabled by default when wildcards are used.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: addMetadata
:Scope: input
:Type: boolean
:Default: auto (on with wildcards)
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Used to control whether file metadata is added to the message object. By
default it is enabled for ``input()`` statements that contain wildcards and
disabled otherwise. This parameter overrides the default behavior.

Input usage
-----------
.. _param-imfile-input-addmetadata:
.. _imfile.parameter.input.addmetadata-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         addMetadata="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

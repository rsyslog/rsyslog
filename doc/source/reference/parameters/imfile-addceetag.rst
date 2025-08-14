.. _param-imfile-addceetag:
.. _imfile.parameter.module.addceetag:

addCeeTag
=========

.. index::
   single: imfile; addCeeTag
   single: addCeeTag

.. summary-start

This is used to turn on or off the addition of the "@cee:" cookie to the message object.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: addCeeTag
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is used to turn on or off the addition of the "@cee:" cookie to the
message object.

Input usage
-----------
.. _param-imfile-input-addceetag:
.. _imfile.parameter.input.addceetag:
.. code-block:: rsyslog

   input(type="imfile" addCeeTag="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

.. _param-imfile-msgdiscardingerror:
.. _imfile.parameter.module.msgdiscardingerror:

msgDiscardingError
==================

.. index::
   single: imfile; msgDiscardingError
   single: msgDiscardingError

.. summary-start

Controls whether an error is logged when a message is truncated.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: msgDiscardingError
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Upon truncation an error is given. When this parameter is turned off, no
error will be shown upon truncation.

Input usage
-----------
.. _param-imfile-input-msgdiscardingerror:
.. _imfile.parameter.input.msgdiscardingerror:
.. code-block:: rsyslog

   input(type="imfile" msgDiscardingError="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

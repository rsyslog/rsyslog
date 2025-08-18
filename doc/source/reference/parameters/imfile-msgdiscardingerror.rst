.. _param-imfile-msgdiscardingerror:
.. _imfile.parameter.input.msgdiscardingerror:
.. _imfile.parameter.msgdiscardingerror:

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
:Default: on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
By default an error message is emitted when a message is truncated. If
``msgDiscardingError`` is ``off``, no error is shown upon truncation.

Input usage
-----------
.. _param-imfile-input-msgdiscardingerror:
.. _imfile.parameter.input.msgdiscardingerror-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         msgDiscardingError="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.

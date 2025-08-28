.. _param-imptcp-defaulttz:
.. _imptcp.parameter.input.defaulttz:

Defaulttz
=========

.. index::
   single: imptcp; Defaulttz
   single: Defaulttz

.. summary-start

Sets a default time zone string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Defaulttz
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set default time zone. At most seven chars are set, as we would otherwise
overrun our buffer.

Input usage
-----------
.. _param-imptcp-input-defaulttz:
.. _imptcp.parameter.input.defaulttz-usage:

.. code-block:: rsyslog

   input(type="imptcp" defaulttz="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.

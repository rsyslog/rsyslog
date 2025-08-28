.. _param-imtcp-address:
.. _imtcp.parameter.input.address:

Address
=======

.. index::
   single: imtcp; Address
   single: Address

.. summary-start

Local address that the listener binds to; ``*`` uses all interfaces.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Address
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
On multi-homed machines, specifies to which local address the listener should be bound.

Input usage
-----------
.. _param-imtcp-input-address:
.. _imtcp.parameter.input.address-usage:

.. code-block:: rsyslog

   input(type="imtcp" address="127.0.0.1" port="514")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

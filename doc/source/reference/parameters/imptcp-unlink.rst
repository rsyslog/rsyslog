.. _param-imptcp-unlink:
.. _imptcp.parameter.input.unlink:

Unlink
======

.. index::
   single: imptcp; Unlink
   single: Unlink

.. summary-start

Unlinks a Unix-domain socket before listening and after closing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Unlink
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If a unix domain socket is being used this controls whether or not the socket
is unlinked before listening and after closing.

Input usage
-----------
.. _param-imptcp-input-unlink:
.. _imptcp.parameter.input.unlink-usage:

.. code-block:: rsyslog

   input(type="imptcp" unlink="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.

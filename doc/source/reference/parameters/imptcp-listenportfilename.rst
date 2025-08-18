.. _param-imptcp-listenportfilename:
.. _imptcp.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imptcp; ListenPortFileName
   single: ListenPortFileName

.. summary-start

Writes the port number being listened on into the specified file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: ListenPortFileName
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.38.0

Description
-----------
With this parameter you can specify the name for a file. In this file the
port, imptcp is connected to, will be written.
This parameter was introduced because the testbench works with dynamic ports.

Input usage
-----------
.. _param-imptcp-input-listenportfilename:
.. _imptcp.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   input(type="imptcp" listenPortFileName="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.

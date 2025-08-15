.. _param-imtcp-listenportfilename:
.. _imtcp.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imtcp; ListenPortFileName
   single: ListenPortFileName

.. summary-start

Writes the listener's port number into the given file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: ListenPortFileName
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: 8.38.0

Description
-----------
Specifies a file name into which the port number this input listens on is written.
It is primarily intended for cases when ``port`` is set to ``0`` to let the OS
assign a free port number. This parameter was introduced for testbench
scenarios that use dynamic ports.

.. note::

   If this parameter is set, ``port="0"`` is permitted. Otherwise, the port
   defaults to ``514``.

Input usage
-----------
.. _param-imtcp-input-listenportfilename:
.. _imtcp.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="0" listenPortFileName="/tmp/imtcp.port")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

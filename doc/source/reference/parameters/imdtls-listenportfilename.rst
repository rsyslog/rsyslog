.. _param-imdtls-listenportfilename:
.. _imdtls.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imdtls; listenPortFileName
   single: listenPortFileName

.. summary-start

Writes the UDP port selected by the operating system when imdtls binds to port
0.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: listenPortFileName
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: v8.2606.0

Description
-----------
When ``port="0"`` is configured, the operating system selects an available UDP
port for the imdtls listener. ``listenPortFileName`` writes that selected port
number to the named file after the socket is bound.

This is primarily useful for tests and automation that must avoid preselecting
a port before the listener starts.

Input usage
-----------
.. _imdtls.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" port="0" listenPortFileName="imdtls.port")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.

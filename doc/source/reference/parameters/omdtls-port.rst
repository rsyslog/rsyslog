.. _param-omdtls-port:
.. _omdtls.parameter.input.port:

port
====

.. index::
   single: omdtls; port
   single: port

.. summary-start

Sets the UDP port on the remote host that receives DTLS messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: port
:Scope: input
:Type: integer (port)
:Default: input=4433
:Required?: yes
:Introduced: v8.2402.0

Description
-----------
Defines the port number on the target host where log messages will be sent.
The default port number for DTLS is 4433.

Input usage
-----------
.. _param-omdtls-input-port:
.. _omdtls.parameter.input.port-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" port="4433")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.

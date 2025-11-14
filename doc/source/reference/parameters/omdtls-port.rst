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
:Type: word
:Default: input=443
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Defines the port number on the target host where log messages will be sent.
If omitted, omdtls defaults to port 443 as compiled into ``omdtls.c``.
The DTLS standard port is 4433, so specify it explicitly if you need the
standard port instead of the built-in default.

Input usage
-----------
.. _omdtls.parameter.input.port-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" port="4433")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.

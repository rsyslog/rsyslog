.. _param-imdtls-port:
.. _imdtls.parameter.input.port:

Port
====

.. index::
   single: imdtls; port
   single: port

.. summary-start


Binds the imdtls listener to the specified UDP port for DTLS traffic.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: port
:Scope: input
:Type: word
:Default: 4433
:Required?: yes
:Introduced: v8.2402.0

Description
-----------
Specifies the UDP port to which the imdtls module will bind and listen for
incoming connections. The default port number for DTLS is 4433.

Input usage
-----------
.. _param-imdtls-input-port:
.. _imdtls.parameter.input.port-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" port="4433")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.

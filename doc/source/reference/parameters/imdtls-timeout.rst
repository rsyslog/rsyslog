.. _param-imdtls-timeout:
.. _imdtls.parameter.input.timeout:

Timeout
=======

.. index::
   single: imdtls; Timeout
   single: Timeout

.. summary-start


Closes idle DTLS sessions after the configured inactivity period.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: Timeout
:Scope: input
:Type: word
:Default: 1800
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Specifies the DTLS session timeout. As DTLS runs on the connectionless UDP protocol, there are no automatic detections of a session timeout. The input closes the DTLS session if no data is received from the client for the configured timeout period. The default is 1800 seconds, which is equal to 30 minutes.

Input usage
-----------
.. _param-imdtls-input-timeout:
.. _imdtls.parameter.input.timeout-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" timeout="900")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.

.. _param-omrelp-localclientip:
.. _omrelp.parameter.input.localclientip:

LocalClientIp
=============

.. index::
   single: omrelp; LocalClientIp
   single: LocalClientIp

.. summary-start

Sets the local client IP address used when connecting to the remote log server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: LocalClientIp
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Omrelp uses ip_address as local client address while connecting to remote logserver.

Input usage
-----------
.. _param-omrelp-input-localclientip:
.. _omrelp.parameter.input.localclientip-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" localClientIp="192.0.2.10")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.

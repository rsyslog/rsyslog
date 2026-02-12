.. _param-impstats-push-tls-certfile:
.. _impstats.parameter.module.push-tls-certfile:

push.tls.certfile
=================

.. index::
   single: impstats; push.tls.certfile
   single: push.tls.certfile

.. summary-start

Specifies the client certificate file used for mutual TLS.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.tls.certfile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Use this together with ``push.tls.keyfile`` when the Remote Write endpoint
requires client certificate authentication.

Module usage
------------
.. _impstats.parameter.module.push-tls-certfile-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.tls.certfile="/etc/rsyslog/certs/impstats-client.crt")

See also
--------
See also :doc:`../../configuration/modules/impstats`.

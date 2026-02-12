.. _param-impstats-push-tls-keyfile:
.. _impstats.parameter.module.push-tls-keyfile:

push.tls.keyfile
================

.. index::
   single: impstats; push.tls.keyfile
   single: push.tls.keyfile

.. summary-start

Specifies the client private key file used for mutual TLS.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.tls.keyfile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Use this together with ``push.tls.certfile`` when the Remote Write endpoint
requires client certificate authentication.

Module usage
------------
.. _impstats.parameter.module.push-tls-keyfile-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.tls.keyfile="/etc/rsyslog/certs/impstats-client.key")

See also
--------
See also :doc:`../../configuration/modules/impstats`.

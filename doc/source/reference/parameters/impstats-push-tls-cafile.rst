.. _param-impstats-push-tls-cafile:
.. _impstats.parameter.module.push-tls-cafile:

push.tls.cafile
===============

.. index::
   single: impstats; push.tls.cafile
   single: push.tls.cafile

.. summary-start

Specifies the CA bundle used to verify the Remote Write TLS certificate.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.tls.cafile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 8.2602.0

Description
-----------
If set, the CA file is used to verify the server certificate when using an
``https://`` push URL. The file must be readable by the rsyslog process.

Module usage
------------
.. _impstats.parameter.module.push-tls-cafile-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.tls.cafile="/etc/ssl/certs/ca-bundle.crt")

See also
--------
See also :doc:`../../configuration/modules/impstats`.

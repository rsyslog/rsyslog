.. _param-imkubernetes-tls-cacert:
.. _imkubernetes.parameter.module.tls-cacert:

tls.caCert
==========

.. index::
   single: imkubernetes; tls.caCert
   single: tls.caCert

.. summary-start

CA certificate bundle used to verify the Kubernetes API TLS certificate;
default ``/var/run/secrets/kubernetes.io/serviceaccount/ca.crt``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: tls.caCert
:Scope: module
:Type: path
:Default: ``/var/run/secrets/kubernetes.io/serviceaccount/ca.crt``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Points libcurl at the CA certificate bundle that should be trusted for the
Kubernetes API endpoint.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" tls.caCert="/var/run/secrets/kubernetes.io/serviceaccount/ca.crt")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

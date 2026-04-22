.. _param-imkubernetes-allowunsignedcerts:
.. _imkubernetes.parameter.module.allowunsignedcerts:

AllowUnsignedCerts
==================

.. index::
   single: imkubernetes; AllowUnsignedCerts
   single: AllowUnsignedCerts

.. summary-start

Disable TLS peer verification for Kubernetes API requests; default ``off``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: AllowUnsignedCerts
:Scope: module
:Type: binary
:Default: ``off``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When enabled, ``imkubernetes`` skips certificate trust validation for
Kubernetes API TLS connections. Use only in controlled test environments.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" AllowUnsignedCerts="on")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

.. _param-imkubernetes-skipverifyhost:
.. _imkubernetes.parameter.module.skipverifyhost:

SkipVerifyHost
==============

.. index::
   single: imkubernetes; SkipVerifyHost
   single: SkipVerifyHost

.. summary-start

Disable TLS hostname verification for Kubernetes API requests; default ``off``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: SkipVerifyHost
:Scope: module
:Type: binary
:Default: ``off``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When enabled, ``imkubernetes`` does not verify that the Kubernetes API server
certificate matches the requested host name. Use only for testing or temporary
workarounds.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" SkipVerifyHost="on")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

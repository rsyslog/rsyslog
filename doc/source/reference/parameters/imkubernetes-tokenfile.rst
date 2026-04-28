.. _param-imkubernetes-tokenfile:
.. _imkubernetes.parameter.module.tokenfile:

TokenFile
=========

.. index::
   single: imkubernetes; TokenFile
   single: TokenFile

.. summary-start

Path to a file containing the Kubernetes API bearer token; default
``/var/run/secrets/kubernetes.io/serviceaccount/token``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: TokenFile
:Scope: module
:Type: path
:Default: ``/var/run/secrets/kubernetes.io/serviceaccount/token``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
If :ref:`Token <param-imkubernetes-token>` is not set, ``imkubernetes`` reads
the first line from this file and uses it as the bearer token for Kubernetes
API requests.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" TokenFile="/var/run/secrets/kubernetes.io/serviceaccount/token")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

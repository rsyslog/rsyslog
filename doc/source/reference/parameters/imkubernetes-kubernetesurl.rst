.. _param-imkubernetes-kubernetesurl:
.. _imkubernetes.parameter.module.kubernetesurl:

KubernetesUrl
=============

.. index::
   single: imkubernetes; KubernetesUrl
   single: KubernetesUrl

.. summary-start

Base URL of the Kubernetes API server; default
``https://kubernetes.default.svc.cluster.local:443``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: KubernetesUrl
:Scope: module
:Type: word
:Default: ``https://kubernetes.default.svc.cluster.local:443``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Sets the API endpoint used when enrichment is enabled. The module queries pod
metadata using ``/api/v1/namespaces/<namespace>/pods/<pod>`` below this base
URL.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" KubernetesUrl="https://localhost:8443")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

.. _param-mmkubernetes-kubernetesurl:
.. _mmkubernetes.parameter.action.kubernetesurl:

KubernetesURL
=============

.. index::
   single: mmkubernetes; KubernetesURL
   single: KubernetesURL

.. summary-start

Specifies the URL of the Kubernetes API server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: KubernetesURL
:Scope: action
:Type: word
:Default: action=https://kubernetes.default.svc.cluster.local:443
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
The URL of the Kubernetes API server. Example: `https://localhost:8443`.

Action usage
------------
.. _param-mmkubernetes-action-kubernetesurl:
.. _mmkubernetes.parameter.action.kubernetesurl-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" kubernetesUrl="https://localhost:8443")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.

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
:Type: array[string]
:Default: https://kubernetes.default.svc.cluster.local:443
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
The URL of the Kubernetes API server. Example: `https://localhost:8443`.

You can specify multiple API server URLs with standard rsyslog array syntax.
mmkubernetes uses the first URL as the primary server, cache identity, statistic
name, and ``master_url`` metadata value. If a transport connection to that URL
fails, mmkubernetes tries the next configured URL for the same metadata lookup.
Kubernetes API responses such as ``404 Not Found`` or ``429 Busy`` are handled
with the normal module semantics and do not by themselves advance to the next
URL. Existing single-URL configurations remain valid because a scalar value is
treated as a one-element array.

Action usage
------------
.. _param-mmkubernetes-action-kubernetesurl:
.. _mmkubernetes.parameter.action.kubernetesurl-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" kubernetesUrl="https://localhost:8443")

   action(type="mmkubernetes"
          kubernetesUrl=["https://k8s-a.example:6443", "https://k8s-b.example:6443"])

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.

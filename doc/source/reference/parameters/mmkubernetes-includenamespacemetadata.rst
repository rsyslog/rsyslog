.. meta::
   :description: Control whether mmkubernetes queries and adds Kubernetes namespace metadata.
   :keywords: rsyslog, mmkubernetes, namespace, metadata, Kubernetes

.. _param-mmkubernetes-includenamespacemetadata:
.. _mmkubernetes.parameter.action.includenamespacemetadata:

includeNamespaceMetadata
========================

.. index::
   single: mmkubernetes; includeNamespaceMetadata
   single: includeNamespaceMetadata

.. summary-start

Controls whether mmkubernetes queries and adds namespace metadata.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: includeNamespaceMetadata
:Scope: module, action
:Type: boolean
:Default: on
:Required?: no
:Introduced: 8.2608.0

Description
-----------

When set to ``off``, mmkubernetes does not query the Kubernetes namespace API
and does not add ``namespace_id``, ``namespace_labels``,
``namespace_annotations``, or the namespace ``creation_timestamp``. The
``namespace_name`` parsed from the input metadata is still added because it is
needed to identify and query the pod.

Disabling namespace metadata can reduce Kubernetes API traffic and avoids
collecting namespace labels and annotations when they are not needed. Pod
metadata collection is unchanged.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmkubernetes" includeNamespaceMetadata="off")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: mmkubernetes
       includeNamespaceMetadata: off

See also
--------

See also :doc:`../../configuration/modules/mmkubernetes`.

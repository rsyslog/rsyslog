.. _param-mmkubernetes-annotation-match:
.. _mmkubernetes.parameter.action.annotation-match:

annotation_match
================

.. index::
   single: mmkubernetes; annotation_match
   single: annotation_match

.. summary-start

Selects pod or namespace annotations whose keys match given patterns.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: annotation_match
:Scope: action
:Type: array
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
By default no pod or namespace annotations will be added to the
messages.  This parameter is an array of patterns to match the keys of
the `annotations` field in the pod and namespace metadata to include
in the `$!kubernetes!annotations` (for pod annotations) or the
`$!kubernetes!namespace_annotations` (for namespace annotations)
message properties.  Example: `["k8s.*master","k8s.*node"]`

Action usage
------------
.. _param-mmkubernetes-action-annotation-match:
.. _mmkubernetes.parameter.action.annotation-match-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" annotationMatch="...")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.

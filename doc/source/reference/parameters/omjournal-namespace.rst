.. _param-omjournal-namespace:
.. _omjournal.parameter.input.namespace:

namespace
=========

.. index::
   single: omjournal; namespace
   single: namespace

.. summary-start

Writes journal entries to a specific systemd journal namespace instead of the default target.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omjournal`.

:Name: namespace
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: Not documented

Description
-----------
Starting from systemd v256, the journal supports namespaces. This allows
you to write to a specific namespace in the journal, which can be useful
for isolating logs from different applications or components.

However, this feature does not support templates yet. If you specify a
namespace, you must not specify a template. If you do, the action will
fail with an error message. Namespaces have to be created before use.

Input usage
-----------
.. _param-omjournal-input-namespace:
.. _omjournal.parameter.input.namespace-usage:

.. code-block:: rsyslog

   action(type="omjournal" namespace="audit-journal-namespace")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

.. _param-omjournal-namespace:
.. _omjournal.parameter.action.namespace:

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
:Scope: action
:Type: word
:Default: none
:Required?: no
:Introduced: 8.2406.0 (requires systemd v256+)

Description
-----------
Starting from systemd v256, the journal supports namespaces. This allows
you to write to a specific namespace in the journal, which can be useful
for isolating logs from different applications or components.

However, this feature has important limitations:

* It is not compatible with templates. If you specify a namespace, you
  must not specify a template. If you do, the action fails with an error
  message.
* Namespaces must be created before use. If a namespace does not exist,
  the action fails and logs an error.

Action usage
------------
.. _param-omjournal-action-namespace:
.. _omjournal.parameter.action.namespace-usage:

.. code-block:: rsyslog

   action(type="omjournal" namespace="audit-journal-namespace")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

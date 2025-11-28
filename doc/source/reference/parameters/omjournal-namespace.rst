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
:Introduced: 8.2406.0 (requires systemd v256+)

Description
-----------
Starting from systemd v256, the journal supports namespaces. This allows
you to write to a specific namespace in the journal, which can be useful
for isolating logs from different applications or components.

However, this feature has important limitations:

* This parameter is mutually exclusive with the ``template`` parameter.
  If both are specified for the same action, the action fails with an error.
* Namespaces must be created before use. If a namespace does not exist,
  the action fails and logs an error. Create namespaces by adding directories
  under ``/var/log/journal/``. For more information, see the
  `systemd-journald.service(8) <https://www.freedesktop.org/software/systemd/man/latest/systemd-journald.service.html>`_
  man page.

Input usage
-----------
.. _param-omjournal-input-namespace:
.. _omjournal.parameter.input.namespace-usage:

.. code-block:: shell

   action(type="omjournal" namespace="audit-journal-namespace")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

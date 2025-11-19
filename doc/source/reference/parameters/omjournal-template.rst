.. _param-omjournal-template:
.. _omjournal.parameter.input.template:

template
========

.. index::
   single: omjournal; template
   single: template

.. summary-start

Selects the template that formats journal entries before they are written.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omjournal`.

:Name: template
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: 8.17.0

Description
-----------
Template to use when submitting messages.

By default, rsyslog will use the incoming ``%msg%`` as the ``MESSAGE`` field
of the journald entry. It also includes ``SYSLOG_IDENTIFIER`` (from the tag),
``SYSLOG_FACILITY``, and ``PRIORITY`` (derived from facility and severity).

You can override the default formatting of the message, and include
custom fields with a template.

.. warning::

   Complex JSON objects or arrays from ``json`` or ``subtree`` templates
   are **not** supported. The omjournal action currently converts each
   field via ``json_object_get_string()``, which returns ``NULL`` for
   nested JSON structures and leads to a crash inside
   ``build_iovec()``. Emit only values that convert cleanly to strings
   (plain text, numbers, booleans) until this limitation is fixed. See
   `rsyslog issue #881 <https://github.com/rsyslog/rsyslog/issues/881>`_
   for background on the crash.

Journald requires that the template's output contains a field named ``MESSAGE``.

Input usage
-----------
.. _param-omjournal-input-template:
.. _omjournal.parameter.input.template-usage:

.. code-block:: shell

   action(type="omjournal" template="journal")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

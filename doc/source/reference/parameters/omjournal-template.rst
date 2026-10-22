.. SPDX-License-Identifier: Apache-2.0

.. _param-omjournal-template:
.. _omjournal.parameter.input.template:

.. meta::
   :description: Controls fields sent by omjournal when writing to the systemd journal.
   :keywords: rsyslog, omjournal, template, PRIORITY, SYSLOG_FACILITY

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

Without a template and without a journal namespace, rsyslog uses the incoming
``$msg`` property as the ``MESSAGE`` field and also includes
``SYSLOG_IDENTIFIER`` (from the tag), ``SYSLOG_FACILITY``, and ``PRIORITY``.
``PRIORITY`` contains the severity alone (0 through 7); the facility is
provided separately in ``SYSLOG_FACILITY``.

When a namespace is selected, the stream API sets ``SYSLOG_IDENTIFIER`` from
the tag and ``PRIORITY`` from the severity. That API does not let omjournal
provide the input facility as ``SYSLOG_FACILITY``, so namespace entries do not
preserve that field. See :doc:`omjournal-namespace` for details.

When a template is specified, only the fields produced by that template are
sent. Use a template to control the message formatting and include custom
fields, including ``PRIORITY`` or ``SYSLOG_FACILITY`` when needed.

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

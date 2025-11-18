.. _param-omjournal-template:
.. _omjournal.parameter.action.template:

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
:Scope: action
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
   (plain text, numbers, booleans) until this limitation is fixed.

Journald requires that you include a template parameter named ``MESSAGE``.

Action usage
------------
.. _param-omjournal-action-template:
.. _omjournal.parameter.action.template-usage:

.. code-block:: rsyslog

   action(type="omjournal" template="journal")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

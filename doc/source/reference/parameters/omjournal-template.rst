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
:Introduced: Not documented

Description
-----------
Template to use when submitting messages.

By default, rsyslog will use the incoming ``%msg%`` as the ``MESSAGE`` field
of the journald entry, and include the syslog tag, facility, and priority.

You can override the default formatting of the message, and include
custom fields with a template. The values of fields from the template's
JSON output are converted to strings before being sent to journald. For
example, a JSON object becomes a JSON-formatted string.

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

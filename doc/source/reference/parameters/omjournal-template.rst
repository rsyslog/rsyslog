.. _param-omjournal-template:
.. _omjournal.parameter.module.template:

Template
========

.. index::
   single: omjournal; Template
   single: Template

.. summary-start

Selects the template that formats journal entries before they are written.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omjournal`.

:Name: Template
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not documented

Description
-----------
Template to use when submitting messages.

By default, rsyslog will use the incoming ``%msg%`` as the ``MESSAGE`` field
of the journald entry, and include the syslog tag and priority.

You can override the default formatting of the message, and include
custom fields with a template. Complex fields in the template
(e.g. json entries) will be added to the journal as json text. Other
fields will be coerced to strings.

Journald requires that you include a template parameter named ``MESSAGE``.

Module usage
------------
.. _param-omjournal-module-template:
.. _omjournal.parameter.module.template-usage:

.. code-block:: rsyslog

   action(type="omjournal" template="journal")

See also
--------
See also :doc:`../../configuration/modules/omjournal`.

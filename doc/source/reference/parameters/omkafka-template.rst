.. _param-omkafka-template:
.. _omkafka.parameter.module.template:

Template
========

.. index::
   single: omkafka; Template
   single: Template

.. summary-start

Template used to format messages for this action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Template
:Scope: action
:Type: word
:Default: action=inherits module
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Sets the template to be used for this action.

Action usage
------------

.. _param-omkafka-action-template:
.. _omkafka.parameter.action.template:
.. code-block:: rsyslog

   action(type="omkafka" Template="MyTemplate")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.


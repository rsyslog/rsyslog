.. _param-omelasticsearch-parent:
.. _omelasticsearch.parameter.module.parent:

parent
======

.. index::
   single: omelasticsearch; parent
   single: parent

.. summary-start

Parent document ID assigned to indexed events.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: parent
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Adds a parent ID to each record. The mapping must define a corresponding parent field.

Action usage
------------
.. _param-omelasticsearch-action-parent:
.. _omelasticsearch.parameter.action.parent:
.. code-block:: rsyslog

   action(type="omelasticsearch" parent="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

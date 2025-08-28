.. _param-omelasticsearch-bulkid:
.. _omelasticsearch.parameter.module.bulkid:

bulkid
======

.. index::
   single: omelasticsearch; bulkid
   single: bulkid

.. summary-start

Unique identifier assigned to each record.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: bulkid
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Specifies a static value or template used as the document `_id`. Required with `writeoperation="create"`.

Action usage
------------
.. _param-omelasticsearch-action-bulkid:
.. _omelasticsearch.parameter.action.bulkid:
.. code-block:: rsyslog

   action(type="omelasticsearch" bulkid="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

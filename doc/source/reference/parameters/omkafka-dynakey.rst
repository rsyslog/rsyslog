.. _param-omkafka-dynakey:
.. _omkafka.parameter.module.dynakey:

DynaKey
=======

.. index::
   single: omkafka; DynaKey
   single: DynaKey

.. summary-start

Treat `key` as a template for dynamic partition keys.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: DynaKey
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.1903

Description
-----------

If set, the key parameter becomes a template for the key to base the
partitioning on.

Action usage
------------

.. _param-omkafka-action-dynakey:
.. _omkafka.parameter.action.dynakey:
.. code-block:: rsyslog

   action(type="omkafka" DynaKey="on" Key="keytemplate")

Notes
-----

- Originally documented as "binary"; uses boolean values.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.


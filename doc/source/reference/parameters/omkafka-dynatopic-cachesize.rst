.. _param-omkafka-dynatopic-cachesize:
.. _omkafka.parameter.module.dynatopic-cachesize:

DynaTopic.Cachesize
===================

.. index::
   single: omkafka; DynaTopic.Cachesize
   single: DynaTopic.Cachesize

.. summary-start

Number of dynamic topics kept in cache.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: DynaTopic.Cachesize
:Scope: action
:Type: integer
:Default: action=50
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, defines the number of topics that will be kept in the dynatopic cache.

Action usage
------------

.. _param-omkafka-action-dynatopic-cachesize:
.. _omkafka.parameter.action.dynatopic-cachesize:
.. code-block:: rsyslog

   action(type="omkafka" DynaTopic.Cachesize="100")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.


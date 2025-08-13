.. _param-omkafka-dynatopic:
.. _omkafka.parameter.module.dynatopic:

DynaTopic
=========

.. index::
   single: omkafka; DynaTopic
   single: DynaTopic

.. summary-start

Treat `topic` as a template for dynamic topic names.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: DynaTopic
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, the topic parameter becomes a template for which topic to
produce messages to. The cache is cleared on HUP.

Action usage
------------

.. _param-omkafka-action-dynatopic:
.. _omkafka.parameter.action.dynatopic:
.. code-block:: rsyslog

   action(type="omkafka" DynaTopic="on" Topic="topic.template")

Notes
-----

- Originally documented as "binary"; uses boolean values.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.


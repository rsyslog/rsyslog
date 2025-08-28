.. _param-omkafka-confparam:
.. _omkafka.parameter.module.confparam:

ConfParam
=========

.. index::
   single: omkafka; ConfParam
   single: ConfParam

.. summary-start

Arbitrary librdkafka producer options `name=value`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: ConfParam
:Scope: action
:Type: array[string]
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Permits specifying Kafka options using ``name=value`` strings.
Parameters are passed directly to librdkafka and support all its producer settings.

Action usage
------------

.. _param-omkafka-action-confparam:
.. _omkafka.parameter.action.confparam:
.. code-block:: rsyslog

   action(type="omkafka" confParam=["compression.codec=snappy"])

Notes
-----

- Multiple values may be set using array syntax.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.


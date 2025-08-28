.. _param-omelasticsearch-timeout:
.. _omelasticsearch.parameter.module.timeout:

timeout
=======

.. index::
   single: omelasticsearch; timeout
   single: timeout

.. summary-start

How long Elasticsearch waits for a primary shard before failing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: timeout
:Scope: action
:Type: word
:Default: action=1m
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Controls the `timeout` query parameter for index operations. Example values use time units such as `1m`.

Action usage
------------
.. _param-omelasticsearch-action-timeout:
.. _omelasticsearch.parameter.action.timeout:
.. code-block:: rsyslog

   action(type="omelasticsearch" timeout="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

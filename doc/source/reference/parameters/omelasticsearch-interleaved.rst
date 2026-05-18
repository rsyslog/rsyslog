.. _param-omelasticsearch-interleaved:
.. _omelasticsearch.parameter.module.interleaved:

interleaved
===========

.. index::
   single: omelasticsearch; interleaved
   single: interleaved

.. summary-start

Interleave request and response data in bulk error-file output.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: interleaved
:Scope: action
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled, omelasticsearch stores request and response data together for
each item written to the configured
:ref:`param-omelasticsearch-errorfile`. This makes it easier to correlate a
bulk request item with the Elasticsearch item response that caused the error
file entry.

The option can be combined with :ref:`param-omelasticsearch-erroronly` to
write only failed bulk items while still keeping each failed request next to
its response.

Action usage
------------
.. _param-omelasticsearch-action-interleaved:
.. _omelasticsearch.parameter.action.interleaved:
.. code-block:: rsyslog

   action(type="omelasticsearch" errorFile="..." interleaved="on")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`,
:ref:`param-omelasticsearch-errorfile`, and
:ref:`param-omelasticsearch-erroronly`.

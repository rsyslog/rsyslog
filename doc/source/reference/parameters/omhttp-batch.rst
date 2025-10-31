.. _param-omhttp-batch:
.. _omhttp.parameter.module.batch:

batch
=====

.. index::
   single: omhttp; batch
   single: batch

.. summary-start

Enables batching so omhttp queues messages and submits them in a single HTTP request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: batch
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
Batch and ``bulkmode`` do the same thing, ``bulkmode`` is included for backwards compatibility. See the :ref:`param-omhttp-batch-format` section for a detailed breakdown of how batching is implemented.

This parameter activates batching mode, which queues messages and sends them as a single request. There are several related parameters that specify the format and size of the batch: they are :ref:`param-omhttp-batch-format`, :ref:`param-omhttp-batch-maxbytes`, and :ref:`param-omhttp-batch-maxsize`.

Note that rsyslog core is the ultimate authority on when a batch must be submitted, due to the way that batching is implemented. This plugin implements the `output plugin transaction interface <https://www.rsyslog.com/doc/v8-stable/development/dev_oplugins.html#output-plugin-transaction-interface>`_. There may be multiple batches in a single transaction, but a batch will never span multiple transactions. This means that if :ref:`param-omhttp-batch-maxsize` or :ref:`param-omhttp-batch-maxbytes` is set very large, you may never actually see batches hit this size. Additionally, the number of messages per transaction is determined by the size of the main, action, and ruleset queues as well.

The plugin flushes a batch early if either the configured :ref:`param-omhttp-batch-maxsize` is reached or if adding the next message would exceed :ref:`param-omhttp-batch-maxbytes` once serialized (format overhead included). When :ref:`param-omhttp-dynrestpath` is enabled, a change of the effective REST path also forces a flush so that each batch targets a single path.

Additionally, due to some open issues with rsyslog and the transaction interface, batching requires some nuanced :ref:`param-omhttp-retry` configuration. By default, omhttp signals transport/server failures to rsyslog core (suspend/resume), which performs retries. The :ref:`param-omhttp-retry-ruleset` mechanism remains available for advanced per-message retry handling in batch mode.

Module usage
------------
.. _param-omhttp-module-batch:
.. _omhttp.parameter.module.batch-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       batch="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.

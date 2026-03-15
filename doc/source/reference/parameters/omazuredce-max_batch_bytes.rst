.. _param-omazuredce-max_batch_bytes:
.. _omazuredce.parameter.action.max_batch_bytes:

.. meta::
   :description: Reference for the omazuredce max_batch_bytes parameter.
   :keywords: rsyslog, omazuredce, max_batch_bytes, azure, batching

max_batch_bytes
===============

.. index::
   single: omazuredce; max_batch_bytes
   single: max_batch_bytes

.. summary-start

Limits the estimated total size of one Azure ingestion request, including the
payload and HTTP overhead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: max_batch_bytes
:Scope: action
:Type: integer
:Default: 1048576
:Required?: no
:Introduced: Not specified

Description
-----------
``max_batch_bytes`` defines the upper bound for a batch. Before appending a new
record, the module estimates the final HTTP request size and flushes the
current batch if needed.

Valid values are in the range ``1`` to ``1048576``. Records that still do not
fit into an otherwise empty batch are logged and dropped.

Action usage
------------
.. _omazuredce.parameter.action.max_batch_bytes-usage:

.. code-block:: rsyslog

   action(type="omazuredce" max_batch_bytes="524288" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.

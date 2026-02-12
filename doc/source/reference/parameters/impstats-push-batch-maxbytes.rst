.. _param-impstats-push-batch-maxbytes:
.. _impstats.parameter.module.push-batch-maxbytes:

push.batch.maxBytes
===================

.. index::
   single: impstats; push.batch.maxBytes
   single: push.batch.maxBytes

.. summary-start

Sets an approximate maximum protobuf size per Remote Write request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.batch.maxBytes
:Scope: module
:Type: integer (bytes)
:Default: module=0
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When set, impstats estimates the protobuf payload size and splits requests into
smaller batches. This is a best-effort limit, not a strict cap.

Module usage
------------
.. _impstats.parameter.module.push-batch-maxbytes-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.batch.maxBytes="500000")

See also
--------
See also :doc:`../../configuration/modules/impstats`.

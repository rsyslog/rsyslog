.. _param-omhttp-batch-maxbytes:
.. _omhttp.parameter.module.batch-maxbytes:

batch.maxbytes
==============

.. index::
   single: omhttp; batch.maxbytes
   single: batch.maxbytes

.. summary-start

Limits the serialized size of each batched HTTP payload.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: batch.maxbytes
:Scope: module
:Type: Size
:Default: module=10485760 (10MB)
:Required?: no
:Introduced: Not specified

Description
-----------
``batch.maxbytes`` and ``maxbytes`` do the same thing, ``maxbytes`` is included for backwards compatibility.

This parameter specifies the maximum size in bytes for each batch.

Module usage
------------
.. _omhttp.parameter.module.batch-maxbytes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       batch="on"
       batchMaxBytes="5m"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.

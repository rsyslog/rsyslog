*********
dyn_inc()
*********

Purpose
=======

dyn_inc(bucket_name_literal_string, str)

Increments counter identified by ``str`` in dyn-stats bucket identified
by ``bucket_name_literal_string``. Returns 0 when increment is successful,
any other return value indicates increment failed.

Counters updated here are reported by **impstats**.

Except for special circumstances (such as memory allocation failing etc),
increment may fail due to metric-name cardinality being under-estimated.
Bucket is configured to support a maximum cardinality (to prevent abuse)
and it rejects increment-operation if it encounters a new(previously unseen)
metric-name(``str``) when full.

**Read more about it here** :doc:`Dynamic Stats<../../configuration/dyn_stats>`


Example
=======

The following example shows the counter $hostname incremented in the bucket
msg_per_host.

.. code-block:: none

   dyn_inc("msg_per_host", $hostname)



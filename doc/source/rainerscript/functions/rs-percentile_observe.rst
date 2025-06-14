********************
percentile_observe()
********************

Purpose
=======

percentile_observe(bucket_name_literal_string, stats_name_literal_string, value)

Adds an observation, identified by the ``bucket_name_literal_string``
and ``stats_name_literal_string``, to the observation set.

Periodically, on an **impstats** reporting interval, percentile and summary metrics are generated
based on the statistics collected.

Except for special circumstances (such as memory allocation failing etc),
observe calls may fail due to stat-name cardinality being under-estimated.
Bucket is configured to support a maximum cardinality (to prevent abuse)
and it rejects observe-operation if it encounters a new(previously unseen)
stat-name(``str``) when full.

**Read more about it here** :doc:`Percentile Stats<../../configuration/percentile_stats>`


Example
=======

In the following example, the ``$msg_count`` value is being recorded as a ``msg_per_host`` statistic in the ``host_statistics`` bucket.


.. code-block:: none

   percentile_observe("host_statistics", "msg_per_host", $msg_count)

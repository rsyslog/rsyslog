Percentile Stats
================

The Percentile Stats component allows user to configure statistical namespaces
(called stats-buckets), which can then be used to record statistical values for
publishing periodic percentiles and summaries.


Percentile Stats Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Percentile-stats configuration involves a **two part setup**. First, define a ``percentile-stats`` bucket and
its attributes. Then, add statistics and observation values to the namespace, by making ``percentile_observation`` calls.

percentile_stats(name="<bucket>", percentiles=[...], windowsize="<count>"...) (object)
-----------------------------------------------------------------------------------------------------------

**Defines** the statistics bucket(identified by the bucket-name) and allows user to set some properties that control behavior of the bucket.

.. code-block::

   percentile_stats(name="bucket_name" percentiles=["95"] windowsize=1000)

Parameters:
    **name** <string literal, mandatory> : Bucket name of a set of statistics to sample.

    **percentiles** <array, mandatory> : A list of strings, with percentile statistic value between 1, 100 inclusive, user would like to publish to impstats.
    This list of percentiles would apply for all statistics tracked under this bucket.

    **windowSize** <number, mandatory> : A max *sliding* window (FIFO) size - rounded *up* to the nearest power of 2, that is larger than the given window size.
    Specifies the maximum number of observations stored over an impstats reporting interval.
    This attribute would apply to all statistics tracked under this bucket.

    **delimiter** <string literal, default: "."> : A single character delimiter used in the published fully qualified statname.
    This delimiter would apply to all statistics tracked under this bucket.


A definition setting all the parameters looks like:

.. code-block::

   percentile_stats(name="host_statistics" percentiles=["50", "95", "99"] windowsize="1000" delimiter="|")


percentile_observe("<bucket>", "<statname>", <value>) (function)
----------------------------------------------------------------

**Adds** a statistical sample to the statistic set identified by the <bucket>, and <statname>.
If the number of values exceed the defined **windowSize**, the earliest recorded value is dropped
and the new value is recorded.

.. note::
  Sums and counts only include values found in the sliding window.
  Min and Max summary values will include all values seen over an observation interval.
  See :ref:`Reporting` below for more info on published summary statistics.



Parameters:
    **bucket name** <string literal, mandatory> : Name of the statistic bucket

    **stat name** <string literal, mandatory> : Name of the statistic to record (this name will be combined with a percentile and reported by impstats to identify the percentile)


    **value** <expression resulting in an integer value, mandatory> : Value to record

A ``percentile_observe`` call looks like:

.. code-block::

   set $.ret = percentile_observe("host_statistics", "msg_per_host", $.mycount);

   if ($.ret != 0) then {
       ....
   }

``$.ret`` captures the error-code. It has value ``0`` when operation is successful and non-zero when it fails. It uses Rsyslog error-codes.


.. _Reporting:

Reporting
^^^^^^^^^

The following example metrics would be reported by **impstats**

Legacy format:

.. code-block::

   ...
   global: origin=percentile host_statistics.new_metric_add=1 host_statistics.ops_overflow=0
   ...
   host_statistics: origin=percentile.bucket msg_per_host|p95=1950 msg_per_host|p50=1500 msg_per_host|p99=1990 msg_per_host|window_min=1001 msg_per_host|window_max=2000 msg_per_host|window_sum=1500500 msg_per_host|window_count=1000
   ...

Json(variants with the same structure are used in other Json based formats such as ``cee`` and ``json-elasticsearch``) format:

.. code-block::

   ...
   { "name": "global", "origin": "percentile", "values": { "host_statistics.new_metric_add": 1, "host_statistics.ops_overflow": 0 } }
   ...
   { "name": "host_statistics", "origin": "percentile.bucket", "values": { "msg_per_host|p95": 1950, "msg_per_host|p50": 1500, "msg_per_host|p99": 1990, "msg_per_host|window_min": 1001, "msg_per_host|window_max": 2000, "msg_per_host|window_sum": 1500500, "msg_per_host|window_count": 1000 } }
   ...

In this case counters are encapsulated inside an object hanging off top-level-key ``values``.

Fields
------

**global: origin=percentile.bucket**:
   **new_metric_add**: Number of "new" metrics added (new counters created).

   **ops_overflow**: Number of operations ignored because number-of-counters-tracked has hit configured max-cardinality.

**host_statistic: origin=percentile.bucket**:
   **msg_per_host|<pXX>**: percentile value, identified by <stat name><delimiter><pXX>, where pXX is one of the requested ``percentiles`` requested.


   **msg_per_host|window_min**: minimum recorded value in the statistical population window. Identified by ``<stat-name><delimiter>window_min``


   **msg_per_host|window_max**: maximum recorded value in the statistical population window. Identified by ``<stat-name><delimiter>window_max``


   **msg_per_host|window_sum**: sum of recorded values in the statistical population window. Identified by ``<stat-name>delimiter>window_sum``


   **msg_per_host|window_count**: count of recorded values in the statistical population window. Identified by ``<stat-name><delimiter>window_count``


Implementation Details
----------------------

Percentile stats module uses a sliding window, sized according to the **windowSize** configuration parameter.
The percentile values are calculated, once every **impstats** interval.
Percentiles are calculated according to the standard “nearest-rank” method:


  n = CEILING((P/100) x N) where:

* n - index to percentile value
* P - percentile [1, 100]
* N - number of values recorded


.. note::
  In order to sort the values, a standard implementation of quicksort is used, which performs pretty well
  on average. However quicksort quickly degrades when there are many repeated elements, thus it is best
  to avoid repeated values if possible.

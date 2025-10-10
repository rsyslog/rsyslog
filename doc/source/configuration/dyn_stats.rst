Dynamic Stats
=============

Rsyslog produces runtime-stats to allow user to study service health, performance, bottlenecks etc. Runtime-stats counters that Rsyslog components publish are statically defined.

**Dynamic Stats** (called dyn-stats henceforth) component allows user to configure stats-namespaces (called stats-buckets) and increment counters within these buckets using RainerScript function call.

The metric-name in this case can be a message-property or a sub-string extracted from message etc.

Dyn-stats configuration
^^^^^^^^^^^^^^^^^^^^^^^

Dyn-stats configuration involves a **two part setup**.

dyn_stats(name="<bucket>"...) (object)
--------------------------------------

**Defines** the bucket(identified by the bucket-name) and allows user to set some properties that control behavior of the bucket.

::

   dyn_stats(name="msg_per_host")

Parameters:
    **name** <string literal, mandatory> : Name of the bucket.

    **resettable** <on|off, default: on> : Whether or not counters should be reset every time they are reported. This works independent of ``resetCounters`` config parameter in :doc:`modules/impstats`.

    **maxCardinality** <number, default: 2000> : Maximum number of unique counter-names to track.

    **unusedMetricLife** <number, default: 3600> : Interval between full purges (in seconds).  This prevents unused counters from occupying resources forever.


A definition setting all the parameters looks like:

::

   dyn_stats(name="msg_per_host" resettable="on" maxCardinality="3000" unusedMetricLife="600")


dyn_inc("<bucket>", <expr>) (function)
--------------------------------------

**Increments** counter identified by value of variable in bucket identified by name.

Parameters:
    **name** <string literal, mandatory> : Name of the bucket
    
    **expr** <expression resulting in a string> : Name of counter (this name will be reported by impstats to identify the counter)
    
A ``dyn_inc`` call looks like:

::

   set $.inc = dyn_inc("msg_per_host", $hostname);
   
   if ($.inc != 0) then {
       ....
   }

``$.inc`` captures the error-code. It has value ``0`` when increment operation is successful and non-zero when it fails. It uses Rsyslog error-codes.

Reporting
^^^^^^^^^

Legacy format:

::

   ...
   global: origin=dynstats msg_per_host.ops_overflow=1 msg_per_host.new_metric_add=3 msg_per_host.no_metric=0 msg_per_host.metrics_purged=0 msg_per_host.ops_ignored=0
   ...
   msg_per_host: origin=dynstats.bucket foo=2 bar=1 baz=1
   ...

Json(variants with the same structure are used in other Json based formats such as ``cee`` and ``json-elasticsearch``) format:

::

   ...
   { "name": "global", "origin": "dynstats", "values": { "msg_per_host.ops_overflow": 1, "msg_per_host.new_metric_add": 3, "msg_per_host.no_metric": 0, "msg_per_host.metrics_purged": 0, "msg_per_host.ops_ignored": 0 } }
   ...
   { "name": "msg_per_host", "origin": "dynstats.bucket", "values": { "foo": 2, "bar": 1, "baz": 1 } }
   ...

In this case counters are encapsulated inside an object hanging off top-level-key ``values``.

Fields
------

**global: origin=dynstats**:
    **ops_overflow**: Number of operations ignored because number-of-counters-tracked has hit configured max-cardinality.

    **new_metric_add**: Number of "new" metrics added (new counters created).

    **no_metric**: Counter-name given was invalid (length = 0).

    **metrics_purged**: Number of counters discarded at discard-cycle (controlled by **unusedMetricLife**).

    **ops_ignored**: Number of operations ignored due to potential performance overhead. Dyn-stats subsystem ignores operations to avoid performance-penalty if it can't get access to counter without delay(lock acquiring latency).

    **purge_triggered**: Indicates that a discard was performed (1 implies a discard-cycle run).
    
**msg_per_host: origin=dynstats.bucket**:
    **<metric_name>**: Value of counter identified by <metric-name>.


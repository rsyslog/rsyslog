RainerScript
============

**RainerScript is a scripting language specifically designed and
well-suited for processing network events and configuring event
processors.**
It is the prime configuration language used for rsyslog.
Please note that RainerScript may not be abbreviated as rscript,
because that's somebody else's trademark.

Some limited RainerScript support is available since rsyslog 3.12.0
(for expression support). In v5, "if .. then" statements are supported.
The first full implementation is available since rsyslog v6.

.. toctree::
   :maxdepth: 2
   
   data_types
   expressions
   functions/index
   control_structures
   configuration_objects
   ratelimit
   constant_strings
   variable_property_types
   lookup_tables
   queue_parameters
   rainerscript_call
   rainerscript_call_indirect
   global
   include

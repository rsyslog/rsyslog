.. meta::
   :description: Overview of the RainerScript configuration language and its components.
   :keywords: rsyslog, rainerscript, configuration language, module functions

.. summary-start

RainerScript is rsyslog's configuration language, including both built-in
functions and module-provided functions such as those under functions/index.

.. summary-end

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
Function support includes built-ins and module functions; see
:doc:`functions/index` for the full list. For quick access, review
:doc:`functions/idx_built-in_functions` and
:doc:`functions/idx_module_functions`.

.. toctree::
   :maxdepth: 2
   
   data_types
   expressions
   functions/index
   control_structures
   configuration_objects
   constant_strings
   variable_property_types
   lookup_tables
   queue_parameters
   rainerscript_call
   rainerscript_call_indirect
   global
   include

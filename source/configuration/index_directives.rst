Legacy Configuration Directives
===============================
All legacy configuration directives need to be specified on a line by their own
and must start with a dollar-sign.

Note that legacy configuration directives that set object options (e.g. for
inputs or actions) only affect those objects that are defined via legacy
constructs. Objects defined via new-style RainerScript objects (e.g.
action(), input()) are **not** affected by legacy directives. The reason
is that otherwise we would again have the ability to mess up a configuration
file with hard to understand constructs. This is avoided by not permitting
to mix and match the way object values are set.

.. toctree::
   :maxdepth: 2

   config_param_types
   global/index
   input_directives/index
   action/index
   ruleset/index

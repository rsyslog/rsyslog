.. meta::
   :description: Overview of RainerScript module functions and how to load them.
   :keywords: rsyslog, rainerscript, module functions, fmpcre, pcre_match

.. summary-start

Module functions are provided by loadable modules, such as fmpcre with
pcre_match() for PCRE-style matching.

.. summary-end

****************
Module Functions
****************

You can make module functions accessible for the configuration by loading the
corresponding module. Once they are loaded, you can use them like any other
rainerscript function. If more than one function are part of the same module,
all functions will be available once the module is loaded.

See also :doc:`Built-in Functions<idx_built-in_functions>` for functions that
are always available without loading a module.


.. toctree::
   :glob:
   :maxdepth: 1

   mo*

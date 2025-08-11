*********
Functions
*********

There are two types of RainerScript functions: built-ins and modules. Built-in
functions can always be called in the configuration. To use module functions,
you have to load the corresponding module first. To do this, add the following
line to your configuration:

::

	module(load="<name of module>")

If more than one function with the same name is present, the function of the
first module loaded will be used. Also, an error message stating this will be
generated. However, the configuration will not abort. Please note that built-in
functions will always be automatically loaded before any modules. Because of this, you
are unable to override any of the built-in functions, since their names are already
in use. The name of a function module starts with fm.


.. toctree::
   :glob:
   :maxdepth: 2

   idx_built-in_functions
   idx_module_functions

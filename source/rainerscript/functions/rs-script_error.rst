**************
script_error()
**************

Purpose
=======

script_error()

Returns the error state of functions that support it. C-Developers note that this
is similar to ``errno`` under Linux. The error state corresponds to the function
immediately called before. The next function call overrides it.

Right now, the value 0 means that the previous functions succeeded, any other
value that it failed. In the future, we may have more fine-grain error codes.

Function descriptions mention if a function supports error state information. If not,
the function call will always set ``script_error()`` to 0.


Example
=======

The following example shows that script_error() only needs to be called and does
not need any parameters.

.. code-block:: none

   script_error()


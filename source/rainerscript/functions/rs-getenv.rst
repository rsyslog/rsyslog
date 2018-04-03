********
getenv()
********

Purpose
=======

getenv(str)

Like the OS call, returns the value of the environment variable,
if it exists. Returns an empty string if it does not exist.


Examples
========

The following example can be used to build a dynamic filter based on
some environment variable:

.. code-block:: none

   if $msg contains getenv('TRIGGERVAR') then /path/to/errfile


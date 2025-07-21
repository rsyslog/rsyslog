********
exists()
********

Purpose
=======

exists($!path!varname)

This function checks if the specified variable exists, in other
words: contains a value. A variable that once was set and later
on unset does also not exist.

The function accepts a single argument, which needs to be a variable.
It returns 0 if the variable does not exist and 1 otherwise. The
function can be combined with any other expression to form more
complex expressions.

.. versionadded:: 8.2010.10


Example
=======

.. code-block:: none

   if exists(!$path!varname) then ...
   if not exists($.local!var) then ...
   if exists($!triggervar) and $msg contains "something" then ...


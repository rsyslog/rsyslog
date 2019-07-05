***************************
previous_action_suspended()
***************************

Purpose
=======

previous_action_suspended()

This boolean function returns 1 (true) if the previous action is suspended,
0 (false) otherwise. It can be used to initiate action that shall happen if
a function failed. Please note that an action failure may not be immediately
detected, so the function return value is a bit fuzzy. It is guaranteed, however
that a suspension will be detected with the next batch of messages that is
being processed.


Example
=======

In the following example the if-clause is executed if the previous action
is suspended.

.. code-block:: none

   action(type="omfwd" protocol="tcp" target="10.1.1.1")

   if previous_action_suspended() then {}



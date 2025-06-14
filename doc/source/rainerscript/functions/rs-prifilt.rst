*********
prifilt()
*********

Purpose
=======

prifilt(constant)

Mimics a traditional PRI-based filter (like "\*.\*" or "mail.info").
The traditional filter string must be given as a **constant string**.
Dynamic string evaluation is not permitted (for performance reasons).


Example
=======

In this example true would be returned on an auth event.

.. code-block:: none

   prifilt("auth,authpriv.*")


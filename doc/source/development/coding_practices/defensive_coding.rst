.. _defensive-coding-and-assertions:

Defensive Coding and Assertions
===============================

**Classification**: Pattern

**Context**: Handling "impossible" states or logic paths that should theoretically never be reached.

**Why it matters**:
Rsyslog prioritizes production stability. We use Static Analysis (SA) and CI to detect bugs. ``assert()`` tells analyzers/AI that a state is invalid. Defensive checks protect production from crashes if the "impossible" happens.

**Steps**:

1.  **Mandatory Assertion**: Always use ``assert()`` to define the invariant. This informs SA tools and AI assistants that this state is invalid.
2.  **Optional Defensive Fallback**: Follow the assert with an ``if`` check to handle the failure gracefully. This is recommended but optional if the fallback handler would be excessively complex or introduce more risk than it solves.
3.  **No UB**: Never use ``__builtin_unreachable()``.
4.  **Order**: Place the ``assert()`` before the defensive check.

**Example**:

.. code-block:: c

   /* 1. Inform SA/AI */
   assert(pPtr != NULL);

   /* 2. Defensive check (optional if complex) */
   if (pPtr == NULL) {
       LogMsg(0, NO_ERRCODE, LOG_ERR, "logic error: pPtr is NULL");
       return;
   }

   /* 3. Logic */
   do_something(pPtr);

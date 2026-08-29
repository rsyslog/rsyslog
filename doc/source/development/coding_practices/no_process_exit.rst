.. _no-process-exit-in-production-code:

.. meta::
   :description: Antipattern forbidding exit() in rsyslog production code and requiring error propagation to the process owner.
   :keywords: rsyslog, coding practice, antipattern, exit, error handling, process lifetime

.. summary-start

Production rsyslog code must never call ``exit()``.  Errors are returned to the
caller so that the process owner can log, unwind, release resources, and decide
whether the daemon should continue or terminate.

.. summary-end

Antipattern: terminating the process with exit()
================================================

**Classification:** Antipattern

**Context:** All production paths in the daemon, runtime, configuration parser,
and loadable modules.  This includes startup and configuration-validation code;
being on an initialization or error path does not make ``exit()`` acceptable.

Why it matters
--------------

A function below the process owner does not have enough context to terminate
the daemon safely.  Calling ``exit()`` bypasses the normal rsyslog error and
rollback contracts, can strand locks or partially constructed generations,
and turns recoverable input such as an invalid reload candidate into service
termination.  It also makes library code unsafe to reuse in validators, tests,
or other host processes.

Required pattern
----------------

- Never add a call to ``exit()`` in production C code.
- Return an ``rsRetVal`` or another explicit error result to the caller.  A
  parser callback that is constrained to a ``void`` signature records the
  error through the parser's established error accumulator instead.
- Unwind ownership locally with the established ``finalize_it`` cleanup path.
- Let the top-level process owner decide whether to continue, reject a request,
  or return a status from ``main()``.
- A forked child that must terminate without running inherited ``atexit``
  handlers uses ``_exit()`` at the child boundary; this is not permission to
  terminate from ordinary runtime or module code.
- Test executables and independent command-line utilities may return a process
  status from ``main()``.  They do not justify ``exit()`` in code linked into
  or called by ``rsyslogd``.

Before (antipattern)
--------------------

.. code-block:: c

   if (includeMissing) {
       LogError(0, RS_RET_FILE_NOT_FOUND, "required include is missing");
       exit(1);
   }

After (required pattern)
------------------------

.. code-block:: c

   if (includeMissing) {
       LogError(0, RS_RET_FILE_NOT_FOUND, "required include is missing");
       return RS_RET_FILE_NOT_FOUND;
   }

Review and migration
--------------------

Existing calls are legacy debt, not precedents.  When a touched path contains
one, replace it with error propagation where the change can be made safely and
add a regression test proving that malformed input is rejected without
terminating the daemon.  Review new and changed production code for ``exit()``
before commit.

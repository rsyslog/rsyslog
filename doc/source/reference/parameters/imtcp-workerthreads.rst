.. _param-imtcp-workerthreads:
.. _imtcp.parameter.module.workerthreads:

WorkerThreads
=============

.. index::
   single: imtcp; WorkerThreads
   single: WorkerThreads

.. summary-start

Sets the default number of worker threads for listeners on epoll-enabled systems.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: WorkerThreads
:Scope: module
:Type: integer
:Default: module=2
:Required?: no
:Introduced: 8.2504.0

Description
-----------
The ``WorkerThreads`` parameter defines the **default number of worker threads** for all ``imtcp``
listeners. This setting applies only on **epoll-enabled systems**. If ``epoll`` is unavailable,
``imtcp`` will always run in **single-threaded mode**, regardless of this setting.

**Default value:** ``2``
**Allowed values:** ``1`` (single-threaded) to any reasonable number (should not exceed CPU cores).

**Behavior and Recommendations**

- If set to ``1``, ``imtcp`` operates in **single-threaded mode**, using the main event loop
  for processing.
- If set to ``2`` or more, a **worker pool** is created, allowing multiple connections to be
  processed in parallel.
- Setting this too high **can degrade performance** due to excessive thread switching.
- A reasonable upper limit is **the number of available CPU cores**.

**Scope and Overrides**
- This is a **module-level parameter**, meaning it **sets the default** for all ``imtcp`` listeners.
- Each listener instance can override this by setting the ``workerthreads`` **listener parameter**.

**Example Configuration**
The following sets a default of **4** worker threads for all listeners, while overriding it to
**8** for a specific listener:

.. code-block:: none

    module(load="imtcp" WorkerThreads="4")  # Default for all listeners

    input(type="imtcp" port="514" workerthreads="8")  # Overrides default, using 8 workers

If ``WorkerThreads`` is not explicitly set, the default of ``2`` will be used.

Module usage
------------
.. _param-imtcp-module-workerthreads:
.. _imtcp.parameter.module.workerthreads-usage:

.. code-block:: rsyslog

   module(load="imtcp" workerThreads="4")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.


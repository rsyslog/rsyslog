.. _param-imtcp-starvationprotection-maxreads:
.. _imtcp.parameter.module.starvationprotection-maxreads:

StarvationProtection.MaxReads
=============================

.. index::
   single: imtcp; StarvationProtection.MaxReads
   single: StarvationProtection.MaxReads

.. summary-start

Limits consecutive reads per connection before switching to another session.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StarvationProtection.MaxReads
:Scope: module
:Type: integer
:Default: module=500
:Required?: no
:Introduced: 8.2504.0

Description
-----------
The ``StarvationProtection.MaxReads`` parameter defines the **maximum number of consecutive
requests** a worker can process for a single connection before switching to another session.
This mechanism prevents any single sender from **monopolizing imtcp's processing capacity**.

**Default value:** ``500``

**Allowed values:**

- ``0`` → Disables starvation protection (a single sender may dominate worker time).
- Any positive integer → Specifies the maximum number of consecutive reads before switching.

**Behavior and Use Cases**

- When a connection continuously sends data, a worker will process it **up to MaxReads times**
  before returning it to the processing queue.
- This ensures that **other active connections** get a chance to be processed.
- Particularly useful in **high-volume environments** where a few senders might otherwise
  consume all resources.
- In **single-threaded mode**, this still provides fairness but cannot fully prevent resource
  exhaustion.

**Scope and Overrides**

- This is a **module-level parameter**, meaning it **sets the default** for all ``imtcp`` listeners.
- Each listener instance can override this by setting the
  ``starvationProtection.maxReads`` **listener parameter**.

**Example Configuration**

The following sets a **default of 300** reads per session before switching to another connection,
while overriding it to **1000** for a specific listener:

.. code-block:: none

    module(load="imtcp" StarvationProtection.MaxReads="300")  # Default for all listeners

    input(type="imtcp" port="514" starvationProtection.MaxReads="1000")  # Overrides default

If ``StarvationProtection.MaxReads`` is not explicitly set, the default of ``500`` will be used.

Module usage
------------
.. _param-imtcp-module-starvationprotection-maxreads:
.. _imtcp.parameter.module.starvationprotection-maxreads-usage:

.. code-block:: rsyslog

   module(load="imtcp" starvationProtection.maxReads="300")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.


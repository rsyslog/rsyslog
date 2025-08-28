.. _param-imptcp-threads:
.. _imptcp.parameter.module.threads:

Threads
=======

.. index::
   single: imptcp; Threads
   single: Threads

.. summary-start

Sets the number of helper threads that pull data off the network.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Threads
:Scope: module
:Type: integer
:Default: module=2
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Number of helper worker threads to process incoming messages. These
threads are utilized to pull data off the network. On a busy system,
additional helper threads (but not more than there are CPUs/Cores)
can help improving performance. The default value is two, which means
there is a default thread count of three (the main input thread plus
two helpers). No more than 16 threads can be set (if tried to,
rsyslog always resorts to 16).

Module usage
------------
.. _param-imptcp-module-threads:
.. _imptcp.parameter.module.threads-usage:

.. code-block:: rsyslog

   module(load="imptcp" threads="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverhelperthreads:

- $InputPTCPServerHelperThreads — maps to Threads (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerHelperThreads
   single: $InputPTCPServerHelperThreads

See also
--------
See also :doc:`../../configuration/modules/imptcp`.

.. _param-imudp-threads:
.. _imudp.parameter.module.threads:

Threads
=======

.. index::
   single: imudp; Threads
   single: Threads

.. summary-start

Number of worker threads receiving data; upper limit 32.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Threads
:Scope: module
:Type: integer
:Default: module=1
:Required?: no
:Introduced: 7.5.5

Description
-----------
Number of worker threads to process incoming messages. These threads are
utilized to pull data off the network. On a busy system, additional threads
(but not more than there are CPUs/Cores) can help improving performance and
avoiding message loss. Note that with too many threads, performance can suffer.
There is a hard upper limit on the number of threads that can be defined.
Currently, this limit is set to 32. It may increase in the future when massive
multicore processors become available.

Module usage
------------
.. _param-imudp-module-threads:
.. _imudp.parameter.module.threads-usage:

.. code-block:: rsyslog

   module(load="imudp" Threads="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

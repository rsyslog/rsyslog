.. _param-imudp-threads:
.. _imudp.parameter.module.threads:

Threads
=======

.. index::
   single: imudp; Threads
   single: Threads

.. summary-start

Number of worker threads that pull UDP data from the network.

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
Specifies how many worker threads process incoming UDP messages. Additional
threads can improve performance on busy systems but should not exceed the number
of CPU cores. A hard upper limit of 32 applies. Too many threads can actually
reduce performance.

Module usage
------------
.. _param-imudp-module-threads:
.. _imudp.parameter.module.threads-usage:
.. code-block:: rsyslog

   module(load="imudp" Threads="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.


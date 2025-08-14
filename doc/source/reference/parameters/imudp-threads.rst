.. _param-imudp-threads:
.. _imudp.parameter.module.threads:

Threads
=======

.. index::
   single: imudp; Threads
   single: Threads

.. summary-start

Defines how many worker threads pull UDP messages for this module.

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
Specifies the number of worker threads dedicated to receiving packets; more threads may improve throughput on busy systems.

Module usage
------------
.. _param-imudp-module-threads:
.. _imudp.parameter.module.threads-usage:
.. code-block:: rsyslog

   module(load="imudp" Threads="...")

Notes
-----
.. versionadded:: 7.5.5

See also
--------
See also :doc:`../../configuration/modules/imudp`.

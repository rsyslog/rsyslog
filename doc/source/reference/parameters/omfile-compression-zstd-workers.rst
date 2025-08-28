.. _param-omfile-compression-zstd-workers:
.. _omfile.parameter.module.compression-zstd-workers:

compression.zstd.workers
========================

.. index::
   single: omfile; compression.zstd.workers
   single: compression.zstd.workers

.. summary-start

In zstd mode, this enables to configure zstd-internal compression worker threads.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: compression.zstd.workers
:Scope: module
:Type: positive-integer
:Default: module=zlib library default
:Required?: no
:Introduced: 8.2208.0

Description
-----------

In zstd mode, this enables to configure zstd-internal compression worker threads.
This setting has nothing to do with rsyslog workers. The zstd library provides
an enhanced worker thread pool which permits multithreaed compression of serial
data streams. Rsyslog fully supports this mode for optimal performance.

Please note that for this parameter to have an effect, the zstd library must
be compiled with multithreading support. As of this writing (2022), this is
**not** the case for many frequently used distros and distro versions. In this
case, you may want to custom install the zstd library with threading enabled. Note
that this does not require a rsyslog rebuild.

Module usage
------------

.. _param-omfile-module-compression-zstd-workers:
.. _omfile.parameter.module.compression-zstd-workers-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" compression.zstd.workers="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.

.. _param-omfile-compression-driver:
.. _omfile.parameter.module.compression-driver:

compression.driver
==================

.. index::
   single: omfile; compression.driver
   single: compression.driver

.. summary-start

For compressed operation ("zlib mode"), this permits to set the compression
driver to be used.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: compression.driver
:Scope: module
:Type: word
:Default: module=zlib
:Required?: no
:Introduced: 8.2208.0

Description
-----------

For compressed operation ("zlib mode"), this permits to set the compression
driver to be used. Originally, only zlib was supported and still is the
default. Since 8.2208.0 zstd is also supported. It provides much better
compression ratios and performance, especially with multiple zstd worker
threads enabled.

Possible values are:
- zlib
- zstd

Module usage
------------

.. _param-omfile-module-compression-driver:
.. _omfile.parameter.module.compression-driver-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" compression.driver="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.

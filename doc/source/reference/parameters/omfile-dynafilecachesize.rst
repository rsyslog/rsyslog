.. _param-omfile-dynafilecachesize:
.. _omfile.parameter.module.dynafilecachesize:

dynaFileCacheSize
=================

.. index::
   single: omfile; dynaFileCacheSize
   single: dynaFileCacheSize

.. summary-start

This parameter specifies the maximum size of the cache for
dynamically-generated file names (dynafile= parameter).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dynaFileCacheSize
:Scope: action
:Type: integer
:Default: action=10
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

This parameter specifies the maximum size of the cache for
dynamically-generated file names (dynafile= parameter).
This setting specifies how many open file handles should
be cached. If, for example, the file name is generated with the hostname
in it and you have 100 different hosts, a cache size of 100 would ensure
that files are opened once and then stay open. This can be a great way
to increase performance. If the cache size is lower than the number of
different files, the least recently used one is discarded (and the file
closed).

Note that this is a per-action value, so if you have
multiple dynafile actions, each of them have their individual caches
(which means the numbers sum up). Ideally, the cache size exactly
matches the need. You can use :doc:`impstats <../../configuration/modules/impstats>` to tune
this value. Note that a too-low cache size can be a very considerable
performance bottleneck.

Action usage
------------

.. _param-omfile-action-dynafilecachesize:
.. _omfile.parameter.action.dynafilecachesize:
.. code-block:: rsyslog

   action(type="omfile" dynaFileCacheSize="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dynafilecachesize:

- $DynaFileCacheSize — maps to dynaFileCacheSize (status: legacy)

.. index::
   single: omfile; $DynaFileCacheSize
   single: $DynaFileCacheSize

See also
--------

See also :doc:`../../configuration/modules/omfile`.

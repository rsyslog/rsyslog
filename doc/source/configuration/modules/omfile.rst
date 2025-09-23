.. _ref-omfile:

**************************
omfile: File Output Module
**************************

===========================  ===========================================================================
**Module Name:**             **omfile**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

The omfile plug-in provides the core functionality of writing messages
to files residing inside the local file system (which may actually be
remote if methods like NFS are used). Both files named with static names
as well files with names based on message content are supported by this
module.


Notable Features
================

- :ref:`omfile-statistic-counter`


Configuration Parameters
========================

Omfile is a built-in module that does not need to be loaded. In order to
specify module parameters, use

.. code-block:: none

   module(load="builtin:omfile" ...parameters...)


Note that legacy parameters **do not** affect new-style RainerScript configuration
objects. See :doc:`basic configuration structure doc <../basic_structure>` to
learn about different configuration languages in use by rsyslog.

.. note::

   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. toctree::
   :hidden:


   ../../reference/parameters/omfile-asyncwriting
   ../../reference/parameters/omfile-addlf
   ../../reference/parameters/omfile-closetimeout
   ../../reference/parameters/omfile-compression-driver
   ../../reference/parameters/omfile-compression-zstd-workers
   ../../reference/parameters/omfile-createdirs
   ../../reference/parameters/omfile-cry-provider
   ../../reference/parameters/omfile-dircreatemode
   ../../reference/parameters/omfile-dirgroup
   ../../reference/parameters/omfile-dirgroupnum
   ../../reference/parameters/omfile-dirowner
   ../../reference/parameters/omfile-dirownernum
   ../../reference/parameters/omfile-dynafile
   ../../reference/parameters/omfile-dynafile-donotsuspend
   ../../reference/parameters/omfile-dynafilecachesize
   ../../reference/parameters/omfile-failonchownfailure
   ../../reference/parameters/omfile-file
   ../../reference/parameters/omfile-filecreatemode
   ../../reference/parameters/omfile-filegroup
   ../../reference/parameters/omfile-filegroupnum
   ../../reference/parameters/omfile-fileowner
   ../../reference/parameters/omfile-fileownernum
   ../../reference/parameters/omfile-flushinterval
   ../../reference/parameters/omfile-flushontxend
   ../../reference/parameters/omfile-iobuffersize
   ../../reference/parameters/omfile-rotation-sizelimit
   ../../reference/parameters/omfile-rotation-sizelimitcommand
   ../../reference/parameters/omfile-sig-provider
   ../../reference/parameters/omfile-sync
   ../../reference/parameters/omfile-template
   ../../reference/parameters/omfile-veryrobustzip
   ../../reference/parameters/omfile-ziplevel

General Notes
-------------

As can be seen in the parameters below, owner and groups can be set either by
name or by direct id (uid, gid). While using a name is more convenient, using
the id is more robust. There may be some situations where the OS is not able
to do the name-to-id resolution, and these cases the owner information will be
set to the process default. This seems to be uncommon and depends on the
authentication provider and service start order. In general, using names
is fine.


Module Parameters
-----------------
.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omfile-template`
     - .. include:: ../../reference/parameters/omfile-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-addlf`
     - .. include:: ../../reference/parameters/omfile-addlf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dircreatemode`
     - .. include:: ../../reference/parameters/omfile-dircreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filecreatemode`
     - .. include:: ../../reference/parameters/omfile-filecreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-fileowner`
     - .. include:: ../../reference/parameters/omfile-fileowner.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-fileownernum`
     - .. include:: ../../reference/parameters/omfile-fileownernum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filegroup`
     - .. include:: ../../reference/parameters/omfile-filegroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filegroupnum`
     - .. include:: ../../reference/parameters/omfile-filegroupnum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirowner`
     - .. include:: ../../reference/parameters/omfile-dirowner.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirownernum`
     - .. include:: ../../reference/parameters/omfile-dirownernum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirgroup`
     - .. include:: ../../reference/parameters/omfile-dirgroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirgroupnum`
     - .. include:: ../../reference/parameters/omfile-dirgroupnum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dynafile-donotsuspend`
     - .. include:: ../../reference/parameters/omfile-dynafile-donotsuspend.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-compression-driver`
     - .. include:: ../../reference/parameters/omfile-compression-driver.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-compression-zstd-workers`
     - .. include:: ../../reference/parameters/omfile-compression-zstd-workers.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Action Parameters
-----------------

Note that **one** of the parameters *file* or *dynaFile* must be specified. This
selects whether a static or dynamic file (name) shall be written to.


.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omfile-template`
     - .. include:: ../../reference/parameters/omfile-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-addlf`
     - .. include:: ../../reference/parameters/omfile-addlf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-fileowner`
     - .. include:: ../../reference/parameters/omfile-fileowner.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-fileownernum`
     - .. include:: ../../reference/parameters/omfile-fileownernum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filegroup`
     - .. include:: ../../reference/parameters/omfile-filegroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filegroupnum`
     - .. include:: ../../reference/parameters/omfile-filegroupnum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirowner`
     - .. include:: ../../reference/parameters/omfile-dirowner.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirownernum`
     - .. include:: ../../reference/parameters/omfile-dirownernum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirgroup`
     - .. include:: ../../reference/parameters/omfile-dirgroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dirgroupnum`
     - .. include:: ../../reference/parameters/omfile-dirgroupnum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-file`
     - .. include:: ../../reference/parameters/omfile-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dynafile`
     - .. include:: ../../reference/parameters/omfile-dynafile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-closetimeout`
     - .. include:: ../../reference/parameters/omfile-closetimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dynafilecachesize`
     - .. include:: ../../reference/parameters/omfile-dynafilecachesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-ziplevel`
     - .. include:: ../../reference/parameters/omfile-ziplevel.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-veryrobustzip`
     - .. include:: ../../reference/parameters/omfile-veryrobustzip.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-flushinterval`
     - .. include:: ../../reference/parameters/omfile-flushinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-asyncwriting`
     - .. include:: ../../reference/parameters/omfile-asyncwriting.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-flushontxend`
     - .. include:: ../../reference/parameters/omfile-flushontxend.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-iobuffersize`
     - .. include:: ../../reference/parameters/omfile-iobuffersize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-filecreatemode`
     - .. include:: ../../reference/parameters/omfile-filecreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-dircreatemode`
     - .. include:: ../../reference/parameters/omfile-dircreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-failonchownfailure`
     - .. include:: ../../reference/parameters/omfile-failonchownfailure.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-createdirs`
     - .. include:: ../../reference/parameters/omfile-createdirs.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-sync`
     - .. include:: ../../reference/parameters/omfile-sync.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-sig-provider`
     - .. include:: ../../reference/parameters/omfile-sig-provider.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-cry-provider`
     - .. include:: ../../reference/parameters/omfile-cry-provider.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-rotation-sizelimit`
     - .. include:: ../../reference/parameters/omfile-rotation-sizelimit.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omfile-rotation-sizelimitcommand`
     - .. include:: ../../reference/parameters/omfile-rotation-sizelimitcommand.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
.. _omfile-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each
dynafile cache. Dynafile cache performance is critical for overall system performance,
so reviewing these counters on a busy system (especially one experiencing performance
problems) is advisable. The statistic is named "dynafile cache", followed by the
template name used for this dynafile action.

The following properties are maintained for each dynafile:

-  **request** - total number of requests made to obtain a dynafile

-  **level0** - requests for the current active file, so no real cache
   lookup needed to be done. These are extremely good.

-  **missed** - cache misses, where the required file did not reside in
   cache. Even with a perfect cache, there will be at least one miss per
   file. That happens when the file is being accessed for the first time
   and brought into cache. So "missed" will always be at least as large
   as the number of different files processed.

-  **evicted** - the number of times a file needed to be evicted from
   the cache as it ran out of space. These can simply happen when
   date-based files are used, and the previous date files are being
   removed from the cache as time progresses. It is better, though, to
   set an appropriate "closeTimeout" (counter described below), so that
   files are removed from the cache after they become no longer accessed.
   It is bad if active files need to be evicted from the cache. This is a
   very costly operation as an evict requires to close the file (thus a
   full flush, no matter of its buffer state) and a later access requires
   a re-open – and the eviction of another file, as the cache obviously has
   run out of free entries. If this happens frequently, it can severely
   affect performance. So a high eviction rate is a sign that the dynafile
   cache size should be increased. If it is already very high, it is
   recommended to re-think about the design of the file store, at least if
   the eviction process causes real performance problems.

-  **maxused** - the maximum number of cache entries ever used. This can
   be used to trim the cache down to a value that’s actually useful but
   does not waste resources. Note that when date-based files are used and
   rsyslog is run for an extended period of time, the cache gradually fills
   up to the max configured value as older files are migrated out of it.
   This will make "maxused" questionable after some time. Frequently enough
   purging the cache can prevent this (usually, once a day is sufficient).

-  **closetimeouts** - available since 8.3.3 – tells how often a file was
   closed due to timeout settings ("closeTimeout" action parameter). These
   are cases where dynafiles or static files have been closed by rsyslog due
   to inactivity. Note that if no "closeTimeout" is specified for the action,
   this counter always is zero. A high or low number in itself doesn’t mean
   anything good or bad. It totally depends on the use case, so no general
   advise can be given.


Caveats/Known Bugs
==================

-  people often report problems that dynafiles are not properly created.
   The common cause for this problem is SELinux rules, which do not permit
   the create of those files (check generated file names and paths!). The
   same happens for generic permission issues (this is often a problem
   under Ubuntu where permissions are dropped by default)

-  One needs to be careful with log rotation if signatures and/or
   encryption are being used. These create side-files, which form a set
   and must be kept together.
   For signatures, the ".sigstate" file must NOT be rotated away if
   signature chains are to be build across multiple files. This is
   because .sigstate contains just global information for the whole file
   set. However, all other files need to be rotated together. The proper
   sequence is to

   #. move all files inside the file set
   #. only AFTER this is completely done, HUP rsyslog

   This sequence will ensure that all files inside the set are
   atomically closed and in sync. HUPing only after a subset of files
   have been moved results in inconsistencies and will most probably
   render the file set unusable.

-  If ``zipLevel`` is greater than 0 and ``veryRobustZip`` is set to off,
   data appended to previously unclean closed files will not be
   accessible with ``gunzip`` if rsyslog writes again in the same
   file. Nonetheless, data is still there and can be correctly accessed
   with other tools like `gztool <https://github.com/circulosmeos/gztool>`_ (v>=1.1) with: ``gztool -p``.


Examples
========

Example 1
---------

The following command writes all syslog messages into a file.

.. code-block:: none

   action(type="omfile" dirCreateMode="0700" FileCreateMode="0644"
          File="/var/log/messages")



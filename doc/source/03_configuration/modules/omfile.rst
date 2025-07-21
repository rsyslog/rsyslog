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

   Parameter names are case-insensitive.


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

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "``$ActionFileDefaultTemplate``"

Set the default template to be used if an action is not configured
to use a specific template.


DirCreateMode
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "FileCreateMode", "0700", "no", "``$DirCreateMode``"

Sets the default dirCreateMode to be used for an action if no
explicit one is specified.


FileCreateMode
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "FileCreateMode", "0644", "no", "``$FileCreateMode``"

Sets the default fileCreateMode to be used for an action if no
explicit one is specified.


fileOwner
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "UID", "process user", "no", "``$FileOwner``"

Sets the default fileOwner to be used for an action if no
explicit one is specified.


fileOwnerNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "process user", "no", "none"

Sets the default fileOwnerNum to be used for an action if no
explicit one is specified.


fileGroup
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "GID", "process user's primary group", "no", "``$FileGroup``"

Sets the default fileGroup to be used for an action if no
explicit one is specified.


fileGroupNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "process user's primary group", "no", "none"

Sets the default fileGroupNum to be used for an action if no
explicit one is specified.


dirOwner
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "UID", "process user", "no", "``$DirOwner``"

Sets the default dirOwner to be used for an action if no
explicit one is specified.


dirOwnerNum
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "process user", "no", "none"

Sets the default dirOwnerNum to be used for an action if no
explicit one is specified.


dirGroup
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "GID", "process user's primary group", "no", "``$DirGroup``"

Sets the default dirGroup to be used for an action if no
explicit one is specified.


dirGroupNum
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "process user's primary group", "no", "none"

Sets the default dirGroupNum to be used for an action if no
explicit one is specified.


dynafile.donotsuspend
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

This permits SUSPENDing dynafile actions. Traditionally, SUSPEND mode was
never entered for dynafiles as it would have blocked overall processing
flow. Default is not to suspend (and thus block).


compression.driver
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "zlib", "no", "none"

.. versionadded:: 8.2208.0

For compressed operation ("zlib mode"), this permits to set the compression
driver to be used. Originally, only zlib was supported and still is the
default. Since 8.2208.0 zstd is also supported. It provides much better
compression ratios and performance, especially with multiple zstd worker
threads enabled.

Possible values are:
- zlib
- zstd


compression.zstd.workers
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "positive integer", "zlib library default", "no", "none"

.. versionadded:: 8.2208.0

In zstd mode, this enables to configure zstd-internal compression worker threads.
This setting has nothing to do with rsyslog workers. The zstd library provides
an enhanced worker thread pool which permits multithreaed compression of serial
data streams. Rsyslog fully supports this mode for optimal performance.

Please note that for this parameter to have an effect, the zstd library must
be compiled with multithreading support. As of this writing (2022), this is
**not** the case for many frequently used distros and distro versions. In this
case, you may want to custom install the zstd library with threading enabled. Note
that this does not require a rsyslog rebuild.


Action Parameters
-----------------

Note that **one** of the parameters *file* or *dynaFile* must be specified. This
selects whether a static or dynamic file (name) shall be written to.


File
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This creates a static file output, always writing into the same file.
If the file already exists, new data is appended to it. Existing
data is not truncated. If the file does not already exist, it is
created. Files are kept open as long as rsyslogd is active. This
conflicts with external log file rotation. In order to close a file
after rotation, send rsyslogd a HUP signal after the file has been
rotated away. Either file or dynaFile can be used, but not both. If both
are given, dynaFile will be used.


dynaFile
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

For each message, the file name is generated based on the given
template. Then, this file is opened. As with the *file* property,
data is appended if the file already exists. If the file does not
exist, a new file is created. The template given in "templateName"
is just a regular :doc:`rsyslog template <../templates>`, so
you have full control over how to format the file name.

To avoid path traversal attacks, *you must make sure that the template
used properly escapes file paths*. This is done by using the *securepath*
parameter in the template's property statements, or the *secpath-drop*
or *secpath-replace* property options with the property replacer.

Either file or dynaFile can be used, but not both. If both are given,
dynaFile will be used.

A cache of recent files is kept. Note
that this cache can consume quite some memory (especially if large
buffer sizes are used). Files are kept open as long as they stay
inside the cache.
Files are removed from the cache when a HUP signal is sent, the
*closeTimeout* occurs, or the cache runs out of space, in which case
the least recently used entry is evicted.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "template set via module parameter", "no", "``$ActionFileDefaultTemplate``"

Sets the template to be used for this action.


closeTimeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "File: 0 DynaFile: 10", "no", "none"

.. versionadded:: 8.3.3

Specifies after how many minutes of inactivity a file is
automatically closed. Note that this functionality is implemented
based on the
:doc:`janitor process <../../02_concepts/janitor>`.
See its doc to understand why and how janitor-based times are
approximate.


dynaFileCacheSize
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10", "no", "``$DynaFileCacheSize``"

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
matches the need. You can use :doc:`impstats <impstats>` to tune
this value. Note that a too-low cache size can be a very considerable
performance bottleneck.


zipLevel
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$OMFileZipLevel``"

If greater than 0, turns on gzip compression of the output file. The
higher the number, the better the compression, but also the more CPU
is required for zipping.


veryRobustZip
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 7.3.0

If *zipLevel* is greater than 0,
then this setting controls if extra headers are written to make the
resulting file extra hardened against malfunction. If set to off,
data appended to previously unclean closed files may not be
accessible without extra tools (like `gztool <https://github.com/circulosmeos/gztool>`_ with: ``gztool -p``).
Note that this risk is usually
expected to be bearable, and thus "off" is the default mode. The
extra headers considerably degrade compression, files with this
option set to "on" may be four to five times as large as files
processed in "off" mode.

**In order to avoid this degradation in compression** both
*flushOnTXEnd* and *asyncWriting* parameters must be set to "off"
and also *ioBufferSize* must be raised from default "4k" value to
at least "32k". This way a reasonable compression factor is
maintained, similar to a non-blocked gzip file:

.. code-block:: none

   veryRobustZip="on" ioBufferSize="64k" flushOnTXEnd="off" asyncWriting="off"


Do not forget to add your desired *zipLevel* to this configuration line.


flushInterval
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "no", "``$OMFileFlushInterval``"

Defines, in seconds, the interval after which unwritten data is
flushed.


asyncWriting
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$OMFileASyncWriting``"

If turned on, the files will be written in asynchronous mode via a
separate thread. In that case, double buffers will be used so that
one buffer can be filled while the other buffer is being written.
Note that in order to enable FlushInterval, AsyncWriting must be set
to "on". Otherwise, the flush interval will be ignored.


flushOnTXEnd
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$OMFileFlushOnTXEnd``"

Omfile has the capability to write output using a buffered writer.
Disk writes are only done when the buffer is full. So if an error
happens during that write, data is potentially lost. Bear in mind that
the buffer may become full only after several hours or a rsyslog
shutdown (however a buffer flush can still be forced by sending rsyslogd
a HUP signal). In cases where this is unacceptable, set FlushOnTXEnd
to "on". Then, data is written at the end of each transaction
(for pre-v5 this means after each log message) and the usual error
recovery thus can handle write errors without data loss.
Note that this option severely reduces the effect of zip compression
and should be switched to "off" for that use case.
Also note that the default -on- is primarily an aid to preserve the
traditional syslogd behaviour.

If you are using dynamic file names (dynafiles), flushes can actually
happen more frequently. In this case, a flush can also happen when
the file name changes within a transaction.


ioBufferSize
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "size", "4 KiB", "no", "``$OMFileIOBufferSize``"

Size of the buffer used to writing output data. The larger the
buffer, the potentially better performance is. The default of 4k is
quite conservative, it is useful to go up to 64k, and 128k if you
used gzip compression (then, even higher sizes may make sense)


dirOwner
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "UID", "system default", "no", "``$DirOwner``"

Set the file owner for directories newly created. Please note that
this setting does not affect the owner of directories already
existing. The parameter is a user name, for which the userid is
obtained by rsyslogd during startup processing. Interim changes to
the user mapping are not detected.


dirOwnerNum
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "``$DirOwnerNum``"

.. versionadded:: 7.5.8

Set the file owner for directories newly created. Please note that
this setting does not affect the owner of directories already
existing. The parameter is a numerical ID, which is used regardless
of whether the user actually exists. This can be useful if the user
mapping is not available to rsyslog during startup.


dirGroup
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "GID", "system default", "no", "``$DirGroup``"

Set the group for directories newly created. Please note that this
setting does not affect the group of directories already existing.
The parameter is a group name, for which the groupid is obtained by
rsyslogd on during startup processing. Interim changes to the user
mapping are not detected.


dirGroupNum
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "``$DirGroupNum``"

Set the group for directories newly created. Please note that this
setting does not affect the group of directories already existing.
The parameter is a numerical ID, which is used regardless of whether
the group actually exists. This can be useful if the group mapping is
not available to rsyslog during startup.


fileOwner
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "UID", "system default", "no", "``$FileOwner``"

Set the file owner for files newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a user name, for which the userid is obtained by
rsyslogd during startup processing. Interim changes to the user
mapping are *not* detected.


fileOwnerNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "``$FileOwnerNum``"

.. versionadded:: 7.5.8

Set the file owner for files newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a numerical ID, which is used regardless of
whether the user actually exists. This can be useful if the user
mapping is not available to rsyslog during startup.


fileGroup
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "GID", "system default", "no", "``$FileGroup``"

Set the group for files newly created. Please note that this setting
does not affect the group of files already existing. The parameter is
a group name, for which the groupid is obtained by rsyslogd during
startup processing. Interim changes to the user mapping are not
detected.


fileGroupNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "``$FileGroupNum``"

.. versionadded:: 7.5.8

Set the group for files newly created. Please note that this setting
does not affect the group of files already existing. The parameter is
a numerical ID, which is used regardless of whether the group
actually exists. This can be useful if the group mapping is not
available to rsyslog during startup.


fileCreateMode
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "equally-named module parameter", "no", "``$FileCreateMode``"

The FileCreateMode directive allows to specify the creation mode
with which rsyslogd creates new files. If not specified, the value
0644 is used (which retains backward-compatibility with earlier
releases). The value given must always be a 4-digit octal number,
with the initial digit being zero.
Please note that the actual permission depend on rsyslogd's process
umask. If in doubt, use "$umask 0000" right at the beginning of the
configuration file to remove any restrictions.


dirCreateMode
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "equally-named module parameter", "no", "``$DirCreateMode``"

This is the same as FileCreateMode, but for directories
automatically generated.


failOnChOwnFailure
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$FailOnCHOwnFailure``"

This option modifies behaviour of file creation. If different owners
or groups are specified for new files or directories and rsyslogd
fails to set these new owners or groups, it will log an error and NOT
write to the file in question if that option is set to "on". If it is
set to "off", the error will be ignored and processing continues.
Keep in mind, that the files in this case may be (in)accessible by
people who should not have permission. The default is "on".


createDirs
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$CreateDirs``"

Create directories on an as-needed basis


sync
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ActionFileEnableSync``"

Enables file syncing capability of omfile.

When enabled, rsyslog does a sync to the data file as well as the
directory it resides after processing each batch. There currently
is no way to sync only after each n-th batch.

Enabling sync causes a severe performance hit. Actually,
it slows omfile so much down, that the probability of loosing messages
**increases**. In short,
you should enable syncing only if you know exactly what you do, and
fully understand how the rest of the engine works, and have tuned
the rest of the engine to lossless operations.


sig.Provider
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "no signature provider", "no", "none"

Selects a signature provider for log signing. By selecting a provider,
the signature feature is turned on.

Currently there is one signature provider available: ":doc:`ksi_ls12 <sigprov_ksi12>`".

Previous signature providers ":doc:`gt <sigprov_gt>`" and ":doc:`ksi <sigprov_ksi>`" are deprecated.


cry.Provider
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "no crypto provider", "no", "none"

Selects a crypto provider for log encryption. By selecting a provider,
the encryption feature is turned on.

Currently, there are two providers called ":doc:`gcry <../cryprov_gcry>`" and ":doc:`ossl <../cryprov_ossl>`".


rotation.sizeLimit
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "size", "0 (disabled)", "no", "`$outchannel` (partly)"

This permits to set a size limit on the output file. When the limit is reached,
rotation of the file is tried. The rotation script needs to be configured via
`rotation.sizeLimitCommand`.

Please note that the size limit is not exact. Some excess bytes are permitted
to prevent messages from being split across two files. Also, a full batch of
messages is not terminated in between. As such, in practice, the size of the
output file can grow some KiB larger than configured.

Also avoid to configure a too-low limit, especially for busy files. Calling the
rotation script is relatively performance intense. As such, it could negatively
affect overall rsyslog performance.


rotation.sizeLimitCommand
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "(empty)", "no", "`$outchannel` (partly)"

This permits to configure the script to be called whe a size limit on the output
file is reached. The actual size limit needs to be configured via
`rotation.sizeLimit`.


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



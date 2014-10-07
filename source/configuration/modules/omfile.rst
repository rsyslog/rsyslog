omfile: File Output Module
==========================

===========================  ===========================================================================
**Module Name:**             **omfile**
**Author:**                  `Rainer Gerhards <http://www.gerhards.net/rainer>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

The omfile plug-in provides the core functionality of writing messages
to files residing inside the local file system (which may actually be
remote if methods like NFS are used). Both files named with static names
as well files with names based on message content are supported by this
module.

Configuration Parameters
------------------------
Omfile is a built-in module that does not need to be loaded. In order to
specify module parameters, use

::

   module(load="builtin:omfile" ...parameters...)

Note that legacy directives **do not** affect new-style RainerScript configuration
objects. See :doc:`basic configuration structure doc <../basic_structure>` to
learn about different configuration languages in use by rsyslog.

General Notes
^^^^^^^^^^^^^

As can be seen in the parameters below, owner and groups can be set either by
name or by direct id (uid, gid). While using a name is more convenient, using
the id is more robust. There may be some situations where the OS is not able
to do the name-to-id resolution, and these cases the owner information will be
set to the process default. This seems to be uncommon and depends on the
authentication provider and service start order. In general, using names
is fine.

Module Parameters
^^^^^^^^^^^^^^^^^

.. function::  template [templateName]

   *Default: RSYSLOG_FileFormat*

   Set the default template to be used if an action is not configured
   to use a specific template.

.. function::  dirCreateMode [octalNumber]

   *Default: 0700*

   Sets the default dirCreateMode to be used for an action if no
   explicit one is specified.

.. function::  fileCreateMode [default 0644] [octalNumber]

   *Default: 0644*

   Sets the default fileCreateMode to be used for an action if no
   explicit one is specified.

.. function:: fileOwner [userName]

   *Default: process user*

   Sets the default fileOwner to be used for an action if no
   explicit one is specified.

.. function:: fileOwnerNum [uid]

   *Default: process user*

   Sets the default fileOwnerNum to be used for an action if no
   explicit one is specified.

.. function:: fileGroup [groupName]

   *Default: process user's primary group*

   Sets the default fileGroup to be used for an action if no
   explicit one is specified.

.. function:: fileGroupNum [gid]

   *Default: process user's primary group*

   Sets the default fileGroupNum to be used for an action if no
   explicit one is specified.

.. function:: dirOwner [userName]

   *Default: process user*

   Sets the default dirOwner to be used for an action if no
   explicit one is specified.

.. function:: dirOwnerNum [uid]

   *Default: process user*

   Sets the default dirOwnerNum to be used for an action if no
   explicit one is specified.

.. function:: dirGroup [groupName]

   *Default: process user's primary group*

   Sets the default dirGroup to be used for an action if no
   explicit one is specified.

.. function:: dirGroupNum [gid]

   *Default: process user's primary group*

   Sets the default dirGroupNum to be used for an action if no
   explicit one is specified.
 

Action Parameters
^^^^^^^^^^^^^^^^^
Note that **one** of the parameters *file* or *dynaFile* must be specified. This
selects whether a static or dynamic file (name) shall be written to.

.. function::  file [fileName]

   *Default: none*

   This creates a static file output, always writing into the same file.
   If the file already exists, new data is appended to it. Existing
   data is not truncated. If the file does not already exist, it is
   created. Files are kept open as long as rsyslogd is active. This
   conflicts with external log file rotation. In order to close a file
   after rotation, send rsyslogd a HUP signal after the file has been
   rotated away.

.. function::  dynaFile [templateName]

   *Default: none*

   For each message, the file name is generated based on the given
   template. Then, this file is opened. As with the \`\`file'' property,
   data is appended if the file already exists. If the file does not
   exist, a new file is created. A cache of recent files is kept. Note
   that this cache can consume quite some memory (especially if large
   buffer sizes are used). Files are kept open as long as they stay
   inside the cache. Currently, files are only evicted from the cache
   when there is need to do so (due to insufficient cache size). To
   force-close (and evict) a dynafile from cache, send a HUP signal to
   rsyslogd.

.. function::  template [templateName]

   *Default: template set via "template" module parameter*

   Sets the template to be used for this action.

.. function::  dynaFileCacheSize [size]

   *Default: 10*

   Applies only if dynamic filenames are used.
   Specifies the number of DynaFiles that will be kept open.
   Note that this is a per-action value, so if you have
   multiple dynafile actions, each of them have their individual caches
   (which means the numbers sum up). Ideally, the cache size exactly
   matches the need. You can use :doc:`impstats <impstats>` to tune
   this value. Note that a too-low cache size can be a very considerable
   performance bottleneck.

.. function::  zipLevel [level]

   *Default: 0*

   if greater 0, turns on gzip compression of the output file. The
   higher the number, the better the compression, but also the more CPU
   is required for zipping.

.. function::  veryRobustZip [switch]

   *Default: on*
   
   *Available since: 7.3.0*

   if *zipLevel* is greater 0,
   then this setting controls if extra headers are written to make the
   resulting file extra hardened against malfunction. If set to off,
   data appended to previously unclean closed files may not be
   accessible without extra tools. Note that this risk is usually
   expected to be bearable, and thus "off" is the default mode. The
   extra headers considerably degrade compression, files with this
   option set to "on" may be four to five times as large as files
   processed in "off" mode.

.. function::  flushInterval [interval]

   *Default: TODO*

   Defines, in seconds, the interval after which unwritten data is
   flushed.

.. function:: asyncWriting [switch]

   *Default: off*

   if turned on, the files will be written in asynchronous mode via a
   separate thread. In that case, double buffers will be used so that
   one buffer can be filled while the other buffer is being written.
   Note that in order to enable FlushInterval, AsyncWriting must be set
   to "on". Otherwise, the flush interval will be ignored. Also note
   that when FlushOnTXEnd is "on" but AsyncWriting is off, output will
   only be written when the buffer is full. This may take several hours,
   or even require a rsyslog shutdown. However, a buffer flush can be
   forced in that case by sending rsyslogd a HUP signal.

.. function::  flushOnTXEnd [switch]

   *Default: on*

   Omfile has the capability to write output using a buffered writer.
   Disk writes are only done when the buffer is full. So if an error
   happens during that write, data is potentially lost. In cases where
   this is unacceptable, set FlushOnTXEnd to on. Then, data is written
   at the end of each transaction (for pre-v5 this means after each log
   message) and the usual error recovery thus can handle write errors
   without data loss. Note that this option severely reduces the effect
   of zip compression and should be switched to off for that use case.
   Note that the default -on- is primarily an aid to preserve the
   traditional syslogd behaviour.

.. function::  ioBufferSize [size]

   *Default: 4 KiB*

   size of the buffer used to writing output data. The larger the
   buffer, the potentially better performance is. The default of 4k is
   quite conservative, it is useful to go up to 64k, and 128K if you
   used gzip compression (then, even higher sizes may make sense)

.. function::  dirOwner [userName]

   *Default: system default*

   Set the file owner for directories newly created. Please note that
   this setting does not affect the owner of directories already
   existing. The parameter is a user name, for which the userid is
   obtained by rsyslogd during startup processing. Interim changes to
   the user mapping are not detected.

.. function::  dirOwnerNum [uid]

   *Default: system default*

   *Available since: 7.5.8, 8.1.4*

   Set the file owner for directories newly created. Please note that
   this setting does not affect the owner of directories already
   existing. The parameter is a numerical ID, which is used regardless
   of whether the user actually exists. This can be useful if the user
   mapping is not available to rsyslog during startup.

.. function::  dirGroup [groupName]

   *Default: system default*

   Set the group for directories newly created. Please note that this
   setting does not affect the group of directories already existing.
   The parameter is a group name, for which the groupid is obtained by
   rsyslogd on during startup processing. Interim changes to the user
   mapping are not detected.

.. function::  dirGroupNum [gid]

   *Default: system default*

   Set the group for directories newly created. Please note that this
   setting does not affect the group of directories already existing.
   The parameter is a numerical ID, which is used regardless of whether
   the group actually exists. This can be useful if the group mapping is
   not available to rsyslog during startup.

.. function::  fileOwner [userName]

   *Default: system default*

   Set the file owner for files newly created. Please note that this
   setting does not affect the owner of files already existing. The
   parameter is a user name, for which the userid is obtained by
   rsyslogd during startup processing. Interim changes to the user
   mapping are *not* detected.

.. function::  fileOwnerNum [uid]

   *Default: system default*

   *Available since: 7.5.8, 8.1.4*

   Set the file owner for files newly created. Please note that this
   setting does not affect the owner of files already existing. The
   parameter is a numerical ID, which which is used regardless of
   whether the user actually exists. This can be useful if the user
   mapping is not available to rsyslog during startup.

.. function::  fileGroup [groupName]

   *Default: system default*

   Set the group for files newly created. Please note that this setting
   does not affect the group of files already existing. The parameter is
   a group name, for which the groupid is obtained by rsyslogd during
   startup processing. Interim changes to the user mapping are not
   detected.

.. function::  fileGroupNum [gid]

   *Default: system default*

   *Available since: 7.5.8, 8.1.4*

   Set the group for files newly created. Please note that this setting
   does not affect the group of files already existing. The parameter is
   a numerical ID, which is used regardless of whether the group
   actually exists. This can be useful if the group mapping is not
   available to rsyslog during startup.

.. function::  fileCreateMode [octalNumber]

   *Default: equally-named module parameter*

   The FileCreateMode directive allows to specify the creation mode
   with which rsyslogd creates new files. If not specified, the value
   0644 is used (which retains backward-compatibility with earlier
   releases). The value given must always be a 4-digit octal number,
   with the initial digit being zero.
   Please note that the actual permission depend on rsyslogd's process
   umask. If in doubt, use "$umask 0000" right at the beginning of the
   configuration file to remove any restrictions.

.. function::  dirCreateMode [octalNumber]

   *Default: equally-named module parameter*

   This is the same as FileCreateMode, but for directories
   automatically generated.

.. function::  failOnChOwnFailuer [switch]

   *Default: equally-named module parameter*

   This option modifies behaviour of file creation. If different owners
   or groups are specified for new files or directories and rsyslogd
   fails to set these new owners or groups, it will log an error and NOT
   write to the file in question if that option is set to "on". If it is
   set to "off", the error will be ignored and processing continues.
   Keep in mind, that the files in this case may be (in)accessible by
   people who should not have permission. The default is "on".

.. function::  createDirs [switch]

   *Default: on*

   create directories on an as-needed basis

.. function::  sync [switch]

   *Default: off*

   enables file syncing capability of omfile.

   Note that this causes a severe performance hit if enabled. Actually,
   it slows omfile so much down, that the probability of loosing messages
   actually increases (except if queue hardening is required). In short,
   you should enable syncing only if you know exactly what you do, and
   fully understand how the rest of the engine works, and have tuned
   the rest of the engine to lossless operations.

.. function::  sig.provider [providerName]

   *Default: no signature provider*

   Selects a signature provider for log signing. By selecting a provider,
   the signature feature is turned on.

   Currently, there only is one provider called ":doc:`gt <sigprov_gt>`". 

.. function::  cry.provider [providerName]

   *Default: no crypto provider*

   Selects a crypto provider for log encryption. By selecting a provider,
   the encryption feature is turned on.

   Currently, there only is one provider called ":doc:`gcry <../cryprov_gcry>`".

See Also
--------

- `Sign log messages through signature provider
  Guardtime <http://www.rsyslog.com/how-to-sign-log-messages-through-signature-provider-guardtime/>`_

Caveats/Known Bugs
------------------

-  people often report problems that dynafiles are not properly created.
   The common cause for this problem is SELinux rules, which do not permit
   the create of those files (check generated file names and pathes!). The
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

Example
-------

The following command writes all syslog messages into a file.

::

  action(type="omfile" dirCreateMode="0700" FileCreateMode="0644"
         File="/var/log/messages")

Legacy Configuration Directives
-------------------------------

Note that the legacy configuration parameters do **not** affect
new-style action definitions via the action() object. This is
by design. To set default for action() objects, use module parameters
in the

::

  module(load="builtin:omfile" ...)

object.

Read about :ref:`the importance of order in legacy configuration<legacy-action-order>`
to understand how to use these configuration directives.
**Legacy directives should NOT be used when writing new configuration files.**

-  **$DynaFileCacheSize**
   equivalent to the "dynaFileCacheSize" parameter
-  **$OMFileZipLevel**
   equivalent to the "zipLevel" parameter
-  **$OMFileFlushInterval**
   equivalent to the "flushInterval" parameter
-  **$OMFileASyncWriting**
   equivalent to the "asyncWriting" parameter
-  **$OMFileFlushOnTXEnd**
   equivalent to the "flushOnTXEnd" parameter
-  **$OMFileIOBufferSize**
   equivalent to the "IOBufferSize" parameter
-  **$DirOwner**
   equivalent to the "dirOwner" parameter
-  **$DirGroup**
   equivalent to the "dirGroup" parameter
-  **$FileOwner**
   equivalent to the "fileOwner" parameter
-  **$FileGroup**
   equivalent to the "fileGroup" parameter
-  **$DirCreateMode**
   equivalent to the "dirCreateMode" parameter
-  **$FileCreateMode**
   equivalent to the "fileCreateMode" parameter
-  **$FailOnCHOwnFailure**
   equivalent to the "failOnChOwnFailure" parameter
-  **$F$OMFileForceCHOwn**
   equivalent to the "ForceChOwn" parameter
-  **$CreateDirs**
   equivalent to the "createDirs" parameter
-  **$ActionFileEnableSync**
   equivalent to the "enableSync" parameter
-  **$ActionFileDefaultTemplate**
   equivalent to the "template" module parameter
-  **$ResetConfigVariables**
   Resets all configuration variables to their default value.

Legacy Sample
^^^^^^^^^^^^^

The following command writes all syslog messages into a file.

::

  $DirCreateMode 0700
  $FileCreateMode 0644
  *.* /var/log/messages

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.

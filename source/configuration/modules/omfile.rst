`back <rsyslog_conf_modules.html>`_

File Output Module
==================

**Module Name:    omfile**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

The omfile plug-in provides the core functionality of writing messages
to files residing inside the local file system (which may actually be
remote if methods like NFS are used). Both files named with static names
as well files with names based on message content are supported by this
module. It is a built-in module that does not need to be loaded.

**Module Parameters**:

-  **Template**\ [templateName]
    Set the default template to be used if an action is not configured
   to use a specific template.
-  **DirCreateMode**\ [default 0700]
    Sets the default DirCreateMode to be used for an action if no
   explicit one is specified.
-  **FileCreateMode**\ [default 0644]
    Sets the default DirCreateMode to be used for an action if no
   explicit one is specified.

- **filecreatemode**
- **fileowner**
- **fileownernum**
- **filegroup**
- **filegroupnum**
- **dirowner**
- **dirownernum**
- **dirgroup**
- **dirgroupnum**
 

**Action Parameters**:

-  **Template**\ [templateName]
    Sets the template to be used for this action. If not specified, the
   default template is applied.
-  **DynaFileCacheSize**\ (not mandatory, default 10)
    Applies only if dynamic filenames are used.
    Specifies the number of DynaFiles that will be kept open. The
   default is 10. Note that this is a per-action value, so if you have
   multiple dynafile actions, each of them have their individual caches
   (which means the numbers sum up). Ideally, the cache size exactly
   matches the need. You can use `impstats <impstats.html>`_ to tune
   this value. Note that a too-low cache size can be a very considerable
   performance bottleneck.
-  **ZipLevel**\ 0..9 [default 0]
    if greater 0, turns on gzip compression of the output file. The
   higher the number, the better the compression, but also the more CPU
   is required for zipping.
-  **VeryRobustZip** [**on**/off] (v7.3.0+) - if ZipLevel is greater 0,
   then this setting controls if extra headers are written to make the
   resulting file extra hardened against malfunction. If set to off,
   data appended to previously unclean closed files may not be
   accessible without extra tools. Note that this risk is usually
   expected to be bearable, and thus "off" is the default mode. The
   extra headers considerably degrade compression, files with this
   option set to "on" may be four to five times as large as files
   processed in "off" mode.
-  **FlushInterval**\ (not mandatory, default will be used)
    Defines, in seconds, the interval after which unwritten data is
   flushed.
-  **ASyncWriting**\ on/off [default off]
    if turned on, the files will be written in asynchronous mode via a
   separate thread. In that case, double buffers will be used so that
   one buffer can be filled while the other buffer is being written.
   Note that in order to enable FlushInterval, AsyncWriting must be set
   to "on". Otherwise, the flush interval will be ignored. Also note
   that when FlushOnTXEnd is "on" but AsyncWriting is off, output will
   only be written when the buffer is full. This may take several hours,
   or even require a rsyslog shutdown. However, a buffer flush can be
   forced in that case by sending rsyslogd a HUP signal.
-  **FlushOnTXEnd**\ on/off [default on]
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
-  **IOBufferSize**\ <size\_nbr>, default 4k
    size of the buffer used to writing output data. The larger the
   buffer, the potentially better performance is. The default of 4k is
   quite conservative, it is useful to go up to 64k, and 128K if you
   used gzip compression (then, even higher sizes may make sense)
-  **DirOwner**
    Set the file owner for directories newly created. Please note that
   this setting does not affect the owner of directories already
   existing. The parameter is a user name, for which the userid is
   obtained by rsyslogd during startup processing. Interim changes to
   the user mapping are not detected.
-  **DirOwnerNum** available in 7.5.8+ and 8.1.4+
    Set the file owner for directories newly created. Please note that
   this setting does not affect the owner of directories already
   existing. The parameter is a numerical ID, which is used regardless
   of whether the user actually exists. This can be useful if the user
   mapping is not available to rsyslog during startup.
-  **DirGroup**
    Set the group for directories newly created. Please note that this
   setting does not affect the group of directories already existing.
   The parameter is a group name, for which the groupid is obtained by
   rsyslogd on during startup processing. Interim changes to the user
   mapping are not detected.
-  **DirGroupNum**
    Set the group for directories newly created. Please note that this
   setting does not affect the group of directories already existing.
   The parameter is a numerical ID, which is used regardless of whether
   the group actually exists. This can be useful if the group mapping is
   not available to rsyslog during startup.
-  **FileOwner**
    Set the file owner for files newly created. Please note that this
   setting does not affect the owner of files already existing. The
   parameter is a user name, for which the userid is obtained by
   rsyslogd during startup processing. Interim changes to the user
   mapping are not detected.
-  **FileOwnerNum** available in 7.5.8+ and 8.1.4+
    Set the file owner for files newly created. Please note that this
   setting does not affect the owner of files already existing. The
   parameter is a numerical ID, which which is used regardless of
   whether the user actually exists. This can be useful if the user
   mapping is not available to rsyslog during startup.
-  **FileGroup**
    Set the group for files newly created. Please note that this setting
   does not affect the group of files already existing. The parameter is
   a group name, for which the groupid is obtained by rsyslogd during
   startup processing. Interim changes to the user mapping are not
   detected.
-  **FileGroupNum** available in 7.5.8+ and 8.1.4+
    Set the group for files newly created. Please note that this setting
   does not affect the group of files already existing. The parameter is
   a numerical ID, which is used regardless of whether the group
   actually exists. This can be useful if the group mapping is not
   available to rsyslog during startup.
-  **FileCreateMode**\ [default equelly-named module parameter]
    The FileCreateMode directive allows to specify the creation mode
   with which rsyslogd creates new files. If not specified, the value
   0644 is used (which retains backward-compatibility with earlier
   releases). The value given must always be a 4-digit octal number,
   with the initial digit being zero.
   Please note that the actual permission depend on rsyslogd's process
   umask. If in doubt, use "$umask 0000" right at the beginning of the
   configuration file to remove any restrictions.
   FileCreateMode may be specified multiple times. If so, it specifies
   the creation mode for all selector lines that follow until the next
   $FileCreateMode directive. Order of lines is vitally important.
-  **DirCreateMode**\ [default equelly-named module parameter]
    This is the same as FileCreateMode, but for directories
   automatically generated.
-  **FailOnCHOwnFailure**\ on/off [default on]
    This option modifies behaviour of file creation. If different owners
   or groups are specified for new files or directories and rsyslogd
   fails to set these new owners or groups, it will log an error and NOT
   write to the file in question if that option is set to "on". If it is
   set to "off", the error will be ignored and processing continues.
   Keep in mind, that the files in this case may be (in)accessible by
   people who should not have permission. The default is "on".
-  **CreateDirs**\ on/off [default on]
    create directories on an as-needed basis
-  **Sync**\ on/off [default off]
    enables file syncing capability of omfile. Note that this causes an
   enormous performance hit if enabled.
-  **File**
    If the file already exists, new data is appended to it. Existing
   data is not truncated. If the file does not already exist, it is
   created. Files are kept open as long as rsyslogd is active. This
   conflicts with external log file rotation. In order to close a file
   after rotation, send rsyslogd a HUP signal after the file has been
   rotated away.
-  **DynaFile**
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
-  **Sig.Provider**\ [ProviderName]
    Selects a signature provider for log signing. Currently, there only
   is one provider called "`gt <sigprov_gt.html>`_\ ".
-  **Cry.Provider**\ [ProviderName]
    Selects a crypto provider for log encryption. Currently, there only
   is one provider called "`gcry <../cryprov_gcry.html>`_\ ".

**See Also**

-  `Sign log messages through signature provider
   Guardtime <http://www.rsyslog.com/how-to-sign-log-messages-through-signature-provider-guardtime/>`_

**Caveats/Known Bugs:**

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

**Sample:**

The following command writes all syslog messages into a file.

action(type="omfile" DirCreateMode="0700" FileCreateMode="0644"
File="/var/log/messages")

**Legacy Configuration Directives**:

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

**Legacy Sample:**

The following command writes all syslog messages into a file.

$ModLoad omfile $DirCreateMode 0700 $FileCreateMode 0644 \*.\*
/var/log/messages

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.

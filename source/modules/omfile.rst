File Output Module
==================

**Module Name**: omfile

**Author:** Rainer Gerhards <rgergards@adiscon.com>

**Description**:

The omfile plug-in provides the core functionality of writing messages
to files residing inside the local file system (which may actually be
remote if methods like NFS are used). Both files named with static names
as well files with names based on message content are supported by this
module. It is a built-in module that does not need to be loaded.


**Module Parameters**:

-  `Template` [templateName]
    Sets a new default template for file actions.

**Action Parameters**:

-  `DynaFileCacheSize` (not mandatory, default will be used)
    Defines a template to be used for the output.

-  `ZipLevel` 0..9 [default 0]
    if greater 0, turns on gzip compression of the output file. The
    higher the number, the better the compression, but also the more CPU
    is required for zipping.

-  `VeryReliableZip` [**on**/off] (v7.3.0+)
    If ZipLevel is than greater 0, then this setting controls if extra headers
    are written to make the resulting file extra hardened against malfunction.
    If set to off, data appended to previously unclean closed files may not be
    accessible without extra tools. Note that this risk is usually
    expected to be bearable, and thus "off" is the default mode. The
    extra headers considerably degrade compression, files with this
    option set to "on" may be four to five times as large as files
    processed in "off" mode.

-  `FlushInterval` (not mandatory, default will be used)
    Defines a template to be used for the output.

-  `ASyncWriting` on/off [default off]
    If turned on, the files will be written in asynchronous mode via a
    separate thread. In that case, double buffers will be used so that
    one buffer can be filled while the other buffer is being written.
    Note that in order to enable FlushInterval, AsyncWriting must be set
    to "on". Otherwise, the flush interval will be ignored. Also note
    that when FlushOnTXEnd is "on" but AsyncWriting is off, output will
    only be written when the buffer is full. This may take several hours,
    or even require a rsyslog shutdown. However, a buffer flush can be
    forced in that case by sending rsyslogd a HUP signal.

-  `FlushOnTXEnd` **on**/off
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

-  `IOBufferSize` <size\_nbr>, default **4k**
    size of the buffer used to writing output data. The larger the
    buffer, the potentially better performance is. The default of 4k is
    quite conservative, it is useful to go up to 64k, and 128K if you
    used gzip compression (then, even higher sizes may make sense)

-  `DirOwner`
    Set the file owner for directories newly created. Please note that
    this setting does not affect the owner of directories already
    existing. The parameter is a user name, for which the userid is
    obtained by rsyslogd during startup processing. Interim changes to
    the user mapping are not detected.

-  `DirGroup`
    Set the group for directories newly created. Please note that this
    setting does not affect the group of directories already existing.
    The parameter is a group name, for which the groupid is obtained by
    rsyslogd on during startup processing. Interim changes to the user
    mapping are not detected.

-  `FileOwner`
    Set the file owner for dynaFiles newly created. Please note that
    this setting does not affect the owner of files already existing. The
    parameter is a user name, for which the userid is obtained by
    rsyslogd during startup processing. Interim changes to the user
    mapping are not detected.

-  `FileGroup`
    Set the group for dynaFiles newly created. Please note that this
    setting does not affect the group of files already existing. The
    parameter is a group name, for which the groupid is obtained by
    rsyslogd during startup processing. Interim changes to the user
    mapping are not detected.

-  `DirCreateMode` [default **0700**]
    This is the same as $FileCreateMode, but for directories
    automatically generated.

-  `FileCreateMode` [default **0644**]
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

-  `FailOnCHOwnFailure` **on**/off
    This option modifies behaviour of dynaFile creation. If different
    owners or groups are specified for new files or directories and
    rsyslogd fails to set these new owners or groups, it will log an
    error and NOT write to the file in question if that option is set to
    "on". If it is set to "off", the error will be ignored and processing
    continues. Keep in mind, that the files in this case may be
    (in)accessible by people who should not have permission. The default
    is "on".

-  `CreateDirs` **on**/off
    create directories on an as-needed basis

-  `Sync` **on**/off
    enables file syncing capability of omfile.

-  `File`
    If the file already exists, new data is appended to it. Existing
    data is not truncated. If the file does not already exist, it is
    created. Files are kept open as long as rsyslogd is active. This
    conflicts with external log file rotation. In order to close a file
    after rotation, send rsyslogd a HUP signal after the file has been
    rotated away.

-  `DynaFile`
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

-  `Sig.Provider` [ProviderName]
    Selects a signature provider for log signing. Currently, there only
    is one provider called "`gt <sigprov_gt.html>`__\ ".

-  `Template` [templateName]
    Sets a new default template for file actions.


**Caveats/Known Bugs:**

-  None.

**Sample:**

The following command writes all syslog messages into a file.

.. code-block :: perl

    Module (load="builtin:omfile")
    *.* action(type="omfile"
               DirCreateMode="0700"
               FileCreateMode="0644"
               File="/var/log/messages")

**Legacy Configuration Directives**:

-  `$DynaFileCacheSize` (not mandatory, default will be used)
    Defines a template to be used for the output.

-  `$OMFileZipLevel` 0..9 [default 0]
    If greater 0, turns on gzip compression of the output file. The
    higher the number, the better the compression, but also the more CPU
    is required for zipping.

-  `$OMFileFlushInterval` (not mandatory, default will be used)
    Defines a template to be used for the output.

-  `$OMFileASyncWriting` **on**/off
    if turned on, the files will be written in asynchronous mode via a
    separate thread. In that case, double buffers will be used so that
    one buffer can be filled while the other buffer is being written.
    Note that in order to enable FlushInterval, AsyncWriting must be set
    to "on". Otherwise, the flush interval will be ignored. Also note
    that when FlushOnTXEnd is "on" but AsyncWriting is off, output will
    only be written when the buffer is full. This may take several hours,
    or even require a rsyslog shutdown. However, a buffer flush can be
    forced in that case by sending rsyslogd a HUP signal.

-  `$OMFileFlushOnTXEnd` **on**/off
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

-  `$OMFileIOBufferSize` <size\_nbr>, default 4k
    size of the buffer used to writing output data. The larger the
    buffer, the potentially better performance is. The default of 4k is
    quite conservative, it is useful to go up to 64k, and 128K if you
    used gzip compression (then, even higher sizes may make sense)

-  `$DirOwner`
    Set the file owner for directories newly created. Please note that
    this setting does not affect the owner of directories already
    existing. The parameter is a user name, for which the userid is
    obtained by rsyslogd during startup processing. Interim changes to
    the user mapping are not detected.

-  `$DirGroup`
    Set the group for directories newly created. Please note that this
    setting does not affect the group of directories already existing.
    The parameter is a group name, for which the groupid is obtained by
    rsyslogd on during startup processing. Interim changes to the user
    mapping are not detected.

-  `$FileOwner`
    Set the file owner for dynaFiles newly created. Please note that
    this setting does not affect the owner of files already existing. The
    parameter is a user name, for which the userid is obtained by
    rsyslogd during startup processing. Interim changes to the user
    mapping are not detected.

-  `$FileGroup`
    Set the group for dynaFiles newly created. Please note that this
    setting does not affect the group of files already existing. The
    parameter is a group name, for which the groupid is obtained by
    rsyslogd during startup processing. Interim changes to the user
    mapping are not detected.

-  `$DirCreateMode` [defaul 0700]
    This is the same as $FileCreateMode, but for directories
    automatically generated.

-  `$FileCreateMode` [default 0644]
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

-  `$FailOnCHOwnFailure` **on**/off
    This option modifies behaviour of dynaFile creation. If different
    owners or groups are specified for new files or directories and
    rsyslogd fails to set these new owners or groups, it will log an
    error and NOT write to the file in question if that option is set to
    "on". If it is set to "off", the error will be ignored and processing
    continues. Keep in mind, that the files in this case may be
    (in)accessible by people who should not have permission. The default
    is "on".

-  `$F$OMFileForceCHOwn`
    force ownership change for all files

-  `$CreateDirs` **on**/off
    create directories on an as-needed basis

-  `$ActionFileEnableSync` **on**/off
    enables file syncing capability of omfile.

-  `$ActionFileDefaultTemplate` [templateName]
    sets a new default template for file actions.

-  `$ResetConfigVariables`
    Resets all configuration variables to their default value. Any
    settings made will not be applied to configuration lines following
    the $ResetConfigVariables. This is a good method to make sure no
    side-effects exists from previous directives. This directive has no
    parameters.

**Legacy Sample:**

The following command writes all syslog messages into a file.

.. code-block :: perl

    $ModLoad omfile $DirCreateMode 0700 $FileCreateMode 0644
    *.* /var/log/messages

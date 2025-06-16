Configuration Formats
=====================

Rsyslog has evolved over several decades. For this reason, it supports three
different configuration formats ("languages"):

-  |FmtBasicName| - previously known as the :doc:`sysklogd  <sysklogd_format>`
   format. This format is best used for expressing basic configurations on a single line,
   stemming from the original syslog.conf format. The most common use case is matching
   on facility/severity and writing matching messages to a log file.

-  |FmtAdvancedName| - previously known as the ``RainerScript`` format. This format,
   first available in rsyslog v6, is the best and most precise format for non-trivial use cases
   where more than one line is needed. This format is designed for advanced use cases
   like forwarding to remote hosts that might be partially offline.

-  |FmtObsoleteName| - previously known as the ``legacy`` format. This format is obsolete
   and should not be used when writing new configurations. It was aimed at small additions
   to the original sysklogd format and has been replaced due to its limitations.

Which Format Should I Use?
--------------------------

For New Configurations
~~~~~~~~~~~~~~~~~~~~~~

Use the |FmtAdvancedName| format for all new configurations due to its flexibility, precision, and control. It handles complex use cases, such as advanced filtering, forwarding, and actions with specific parameters.

Existing Configurations in Basic Format
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some distributions ship default configurations in |FmtBasicName| format. These configurations are simple to convert to the |FmtAdvancedName| format, which is suggested if you plan to add complex constructs, like rulesets or actions with queues.

Retaining Basic Format
~~~~~~~~~~~~~~~~~~~~~~

Continue using the |FmtBasicName| format if there is a strong reliance on external documentation describing the |FmtBasicName| format or if there are many existing configurations in that format. This format is simple and widely understood, making it adequate for basic logging needs.

Example - Basic Format
^^^^^^^^^^^^^^^^^^^^^^

:: 

    mail.info /var/log/mail.log
    mail.err @@server.example.net

Advanced Use Cases
~~~~~~~~~~~~~~~~~~

For anything beyond basic logging, use the |FmtAdvancedName| format. Advantages include:

- Fine control over rsyslog operations via advanced parameters
- Easy to follow block structure
- Easy to write and maintain
- Safe for use with include files

Example - Advanced Format
^^^^^^^^^^^^^^^^^^^^^^^^^

:: 

    mail.err action(type="omfwd" protocol="tcp" queue.type="linkedList")

Deprecated Format
~~~~~~~~~~~~~~~~~

**Do not use |FmtObsoleteName| format. It is obsolete and will make your life difficult.** This format is only supported to maintain compatibility with very old configurations. Users are strongly encouraged to migrate to the |FmtBasicName| or |FmtAdvancedName| formats as appropriate.

Conclusion
----------

For new configurations or complex logging needs, the |FmtAdvancedName| format is the best choice. The |FmtBasicName| format should only be retained if there is a compelling reason, such as existing configurations or reliance on specific external documentation.

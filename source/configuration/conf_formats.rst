Configuration Formats
=====================

Rsyslog has evolved over several decades. For this reason it supports three
different configuration formats ("languages"):

-  |FormatsBasicNewName| - previously known as the :doc:`sysklogd  <sysklogd_format>`
   format, this configuration format is widely known, actively supported and
   occasionally enhanced with new features. This format is recommended for simple
   use cases where the entire statement cleanly fits on a single line. Note
   that some few constructs are no longer supported because they are incompatible
   with newer features. These are mentioned in the compatibility docs.

-  |FormatsAdvancedNewName| - previously known as "|FormatsAdvancedOldName|",
   this is the current, best and most precise format to use for non-trivial use
   cases where more than one line is needed. This format is available in
   rsyslog v6 and above.

-  |FormatsObsoleteNewName| - previously known as the |FormatsObsoleteOldName|
   format, this format was created in the early days where we expected that
   rsyslog would extend syslklogd just mildly. Consequently, it was primarily
   aimed at small additions to the original sysklogd format. In essence,
   everything that needs to be written on a single line that starts with a
   dollar sign is legacy format. This format is notoriously difficult to use
   and should not be used for new configurations. Users of this format
   are encouraged to migrate to the |FormatsBasicNewName| or
   |FormatsAdvancedNewName| formats.

Which Format should I Use?
~~~~~~~~~~~~~~~~~~~~~~~~~~

Rsyslog supports all three formats concurrently, so you can pick either
|FormatsBasicNewName| or |FormatsAdvancedNewName| format.

**It is recommended to use** |FormatsAdvancedNewName| **format.** Advantages are:

- statements are self-contained; all parameters are given at exactly the
  place where they belong to
- easy to follow block structure
- easy to write
- safe for use with include files


For *very simple things* |FormatsBasicNewName| statement types are still
suggested, especially if the full config consists of such simple things. The
classical sample is writing to files (or forwarding) via priority.
In sysklogd, this looks like::

   mail.info /var/log/mail.log
   mail.err @server.example.net

This is hard to beat in simplicity, still being taught in courses
and a lot of people know this syntax. It is perfectly fine to use
these constructs even in newly written config files. Note that many
distributions use this format in their default rsyslog.conf.

**Do not use** |FormatsObsoleteNewName| **format. It will make your life
miserable.**
The |FormatsObsoleteNewName| format is primarily supported in order
not to break existing configurations.

.. toctree::
   :maxdepth: 2

   sysklogd_format

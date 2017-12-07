Configuration Formats
=====================

Rsyslog has evolved over several decades. For this reason it supports three
different configuration formats ("languages"):

-  **sysklogd** - this is the plain old syslog.conf format, in use since
   the early days of system logging. It is taught everywhere and
   still pretty useful for simple use cases. Note that some few
   constructs are no longer supported because they are incompatible with
   newer features. These are mentioned in the compatibility docs.
-  **RainerScript** - the new style format. This is the best and most
   precise format to be used for more complex cases. This format is available
   in rsyslog v6 and above.
-  **legacy rsyslog** - this format was created in the early days where
   we expected that rsyslog would extend syslklogd just mildly.
   Consequently, it was primarily aimed at small additions to the original
   sysklogd format. In essence, everything that needs to be written on a
   single line that starts with a dollar sign is legacy format.


Which Format should I Use?
~~~~~~~~~~~~~~~~~~~~~~~~~~

Rsyslog supports all three formats concurrently, so you can pick either
sysklogd or RainerScript format.

In general it is recommended to use RainerScript format. Advantages are:

- statements are self-contained; all parameters are given at exactly the
  place where they belong to
- easy to follow block structure
- easy to write
- safe for use with include files


For *very simple things* sysklogd statement types are still suggested,
especially if the full config consists of such simple things. The
classical sample is writing to files (or forwarding) via priority.
In sysklogd, this looks like::

   mail.info /var/log/mail.log
   mail.err @server.example.net

This is hard to beat in simplicity, still being taught in courses
and a lot of people know this syntax. It is perfectly fine to use
these constructs even in newly written config files. Note that many
distributions use this format in their default rsyslog.conf.

**Do not use rsyslog legacy format. It will make your life miserable.**
The legacy format is primarily supported in order not to break existing
configurations.

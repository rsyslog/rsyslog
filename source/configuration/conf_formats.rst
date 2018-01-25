Configuration Formats
=====================

Rsyslog has evolved over several decades. For this reason it supports three
different configuration formats ("languages"):

-  |FmtBasicName| - previously known as the :doc:`sysklogd  <sysklogd_format>`
   format, this is the format best used to express basic things, such as where
   the statement fits on a single line. It stems back to the original
   syslog.conf format, in use now for several decades.

   The most common use case is matching on facility/severity and writing
   matching messages to a log file.

-  |FmtAdvancedName| - previously known as the ``RainerScript`` format, this
   format was first available in rsyslog v6 and is the current, best and most
   precise format for non-trivial use cases where more than one line is needed.

   Prior to v7, there was a performance impact when using this format that
   encouraged use of the |FmtBasicName| format for best results. Current
   versions of rsyslog do not suffer from this (historical) performance impact.

   This new style format is specifically targeted towards more advanced use
   cases like forwarding to remote hosts that might be partially offline.

-  |FmtObsoleteName| - previously known simply as the ``legacy`` format, this
   format is exactly what its name implies: it is obsolete and should not
   be used when writing new configurations. It was created in the early
   days (up to rsyslog version 5) where we expected that rsyslog would extend
   sysklogd just mildly.  Consequently, it was primarily aimed at small
   additions to the original sysklogd format. Practice has shown that it
   was notoriously hard to use for more advanced use cases, and thus we
   replaced it with the |FmtAdvancedName| format.

   In essence, everything that needs to be written on a single line that
   starts with a dollar sign is legacy format. Users of this format are
   encouraged to migrate to the |FmtBasicName| or |FmtAdvancedName| formats.

Which Format should I Use?
~~~~~~~~~~~~~~~~~~~~~~~~~~

While rsyslog supports all three formats concurrently, you are **strongly**
encouraged to avoid using the |FmtObsoleteName| format. Instead, you should
use the |FmtBasicName| format for basic configurations and the |FmtAdvancedName|
format for anything else.

While it is an older format, the |FmtBasicName| format is still suggested for
configurations that mostly consist of simple statements. The classic
example is writing to files (or forwarding) via priority. In |FmtBasicName|
format, this looks like:

::

   mail.info /var/log/mail.log
   mail.err @@server.example.net

This is hard to beat in simplicity, still being taught in courses
and a lot of people know this syntax. It is perfectly fine to use
these constructs even in newly written config files. Note that many
distributions use this format in their default rsyslog.conf, so you will
likely find it in existing configurations.

**For anything more advanced, use** the |FmtAdvancedName| format. Advantages are:

- fine control over rsyslog operations via advanced parameters
- easy to follow block structure
- easy to write
- safe for use with include files

To continue with the above example, the |FmtAdvancedName| format is preferrable
if you want to make sure that an offline remote destination will not slow down
local log file writing. In that case, forwarding is done via:

::

   mail.err action(type="omfwd" protocol="tcp" queue.type="linkedList")

As can be seen by this example, the |FmtAdvancedName| format permits specifing
additional parameters to fine tune the behavior, whereas the |FmtBasicName|
format does not provide this level of control.

**Do not use** |FmtObsoleteName| **format. It will make your life
miserable.** It is primarily supported in order to not break existing
configurations.

Whatever you can do with the |FmtObsoleteName| format, you can also do
with the |FmtAdvancedName| format. The opposite is not true: Many newer features
cannot be turned on via the |FmtObsoleteName| format. The |FmtObsoleteName|
format is hard to understand and hard to get right. As you may inherit a rsyslog
configuration that makes use of it, this documentation gives you some clues
of what the obsolete statements do. For full details, obtain a
`v5 version of the rsyslog
documentation <http://www.rsyslog.com/doc/v5-stable/index.html>`_ (yes, this
format is dead since 2010!).

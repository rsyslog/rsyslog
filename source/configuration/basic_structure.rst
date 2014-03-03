Basic Structure
===============
Rsyslog supports standard sysklogd's configuration file format and
extends it. So in general, you can take a "normal" syslog.conf and use
it together with rsyslogd. It will understand everything. However, to
use most of rsyslogd's unique features, you need to add extended
configuration directives.

Rsyslogd supports the classical, selector-based rule lines. They are
still at the heart of it and all actions are initiated via rule lines. A
rule lines is any line not starting with a $ or the comment sign (#).
Lines starting with $ carry rsyslog-specific directives.

Every rule line consists of two fields, a selector field and an action
field. These two fields are separated by one or more spaces or tabs. The
selector field specifies a pattern of facilities and priorities
belonging to the specified action.

Lines starting with a hash mark ("#'') and empty lines are ignored.
 
Configuration File Syntax Differences
-------------------------------------
Rsyslogd uses a slightly different syntax for its configuration file
than the original BSD sources. Originally all messages of a specific
priority and above were forwarded to the log file. The modifiers "='',
"!'' and "!-'' were added to make rsyslogd more flexible and to use it
in a more intuitive manner.

The original BSD syslogd doesn't understand spaces as separators
between the selector and the action field.

When compared to syslogd from sysklogd package, rsyslogd offers
additional `features <features.html>`_ (like template and database
support). For obvious reasons, the syntax for defining such features is
available in rsyslogd, only.

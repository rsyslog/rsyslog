Filter Conditions
=================

Rsyslog offers four different types "filter conditions":

-  "traditional" severity and facility based selectors
-  property-based filters
-  expression-based filters
-  BSD-style blocks (not upward compatible)

Selectors
---------

**Selectors are the traditional way of filtering syslog messages.** They
have been kept in rsyslog with their original syntax, because it is
well-known, highly effective and also needed for compatibility with
stock syslogd configuration files. If you just need to filter based on
priority and facility, you should do this with selector lines. They are
**not** second-class citizens in rsyslog and offer the simplest syntax
for this job. In versions of rsyslog prior to v7 there were significant
performance gains by using selector lines instead of the |FmtAdvancedName|
format. There is no longer any difference in performance between the two
formats.

The selector field itself again consists of two parts, a facility and a
priority, separated by a period (".''). Both parts are case insensitive
and can also be specified as decimal numbers, but don't do that, you
have been warned. Both facilities and priorities are described in
syslog(3). The names mentioned below correspond to the similar
LOG\_-values in /usr/include/syslog.h.

The facility is one of the following keywords: auth, authpriv, cron,
daemon, kern, lpr, mail, mark, news, security (same as auth), syslog,
user, uucp and local0 through local7. The keyword security should not be
used anymore and mark is only for internal use and therefore should not
be used in applications. Anyway, you may want to specify and redirect
these messages here. The facility specifies the subsystem that produced
the message, i.e. all mail programs log with the mail facility
(LOG\_MAIL) if they log using syslog.

The priority is one of the following keywords, in ascending order:
debug, info, notice, warning, warn (same as warning), err, error (same
as err), crit, alert, emerg, panic (same as emerg). The keywords error,
warn and panic are deprecated and should not be used anymore. The
priority defines the severity of the message.

The behavior of the original BSD syslogd is that all messages of the
specified priority and higher are logged according to the given action.
Rsyslogd behaves the same, but has some extensions.

In addition to the above mentioned names the rsyslogd(8) understands
the following extensions: An asterisk ("\*'') stands for all facilities
or all priorities, depending on where it is used (before or after the
period). The keyword none stands for no priority of the given facility.
You can specify multiple facilities with the same priority pattern in
one statement using the comma (",'') operator. You may specify as much
facilities as you want. Remember that only the facility part from such a
statement is taken, a priority part would be skipped.

Multiple selectors may be specified for a single action using the
semicolon (";'') separator. Remember that each selector in the selector
field is capable to overwrite the preceding ones. Using this behavior
you can exclude some priorities from the pattern.

Rsyslogd has a syntax extension to the original BSD source, that makes
the use of selectors use more intuitively. You may precede every priority
with an equals
sign ("='') to specify only this single priority and not any of the
above. You may also (both is valid, too) precede the priority with an
exclamation mark ("!'') to ignore all that priorities, either exact this
one or this and any higher priority. If you use both extensions then the
exclamation mark must occur before the equals sign, just use it
intuitively.

However, please note that there are some restrictions over the traditional
BSD syslog behaviour. These restrictions stem back to sysklogd, exist
probably since at least the 1990's and as such have always been in
rsyslog.

Namely, in BSD syslogd you can craft a selector like this:

\*.debug;local6.err

The intent is to log all facilities at debug or higher, except for local6,
which should only log at err or higher.

Unfortunately, local6.err will permit error severity and higher, but will
*not* exclude lower severity messages from facility local6.

As an alternative, you can explicitly exclude all severities that you do
not want to match. For the above case, this selector is equivalent to the
BSD syslog selector:

\*.debug;local6.!=info;local6.!=notice;local6.!=warn

An easier approach is probably to do if ... then based matching in script.

Property-Based Filters
----------------------

Property-based filters are unique to rsyslogd. They allow to filter on
any property, like HOSTNAME, syslogtag and msg. A list of all
currently-supported properties can be found in the :doc:`rsyslog properties
documentation <properties>`. With this filter, each property can be checked
against a specified value, using a specified compare operation.

A property-based filter must start with a colon **in column 1**. This tells
rsyslogd that it is the new filter type. The colon must be followed by
the property name, a comma, the name of the compare operation to carry
out, another comma and then the value to compare against. This value
must be quoted. There can be spaces and tabs between the commas.
Property names and compare operations are case-sensitive, so "msg"
works, while "MSG" is an invalid property name. In brief, the syntax is
as follows:

``:property, [!]compare-operation, "value"``

Compare-Operations
~~~~~~~~~~~~~~~~~~

The following **compare-operations** are currently supported:

**contains**
  Checks if the string provided in value is contained in the property.
  There must be an exact match, wildcards are not supported.

**isequal**
  Compares the "value" string provided and the property contents. These
  two values must be exactly equal to match. The difference to contains is
  that contains searches for the value anywhere inside the property value,
  whereas all characters must be identical for isequal. As such, isequal
  is most useful for fields like syslogtag or FROMHOST, where you probably
  know the exact contents.

**startswith**
  Checks if the value is found exactly at the beginning of the property
  value. For example, if you search for "val" with

  ``:msg, startswith, "val"``

  it will be a match if msg contains "values are in this message" but it
  won't match if the msg contains "There are values in this message" (in
  the later case, *"contains"* would match). Please note that "startswith" is
  by far faster than regular expressions. So even once they are
  implemented, it can make very much sense (performance-wise) to use
  "startswith".

**endswith**
  Checks if the value appears exactly at the end of the property value.
  For example, ``:programname, endswith, "_foo"`` matches if the program name
  ends with ``_foo``.

**regex**
  Compares the property against the provided POSIX BRE regular expression.

**ereregex**
  Compares the property against the provided POSIX ERE regular expression.

You can use the bang-character (!) immediately in front of a
compare-operation, the outcome of this operation is negated. For
example, if msg contains "This is an informative message", the following
sample would not match:

``:msg, contains, "error"``

but this one matches:

``:msg, !contains, "error"``

Using negation can be useful if you would like to do some generic
processing but exclude some specific events. You can use the discard
action in conjunction with that. A sample would be:

::

  *.* /var/log/allmsgs-including-informational.log
  :msg, contains, "informational"  ~
  *.* /var/log/allmsgs-but-informational.log

Do not overlook the tilde in line 2! In this sample, all messages
are written to the file allmsgs-including-informational.log. Then, all
messages containing the string "informational" are discarded. That means
the config file lines below the "discard line" (number 2 in our sample)
will not be applied to this message. Then, all remaining lines will also
be written to the file allmsgs-but-informational.log.

Value Part
~~~~~~~~~~

**Value** is a quoted string. It supports some escape sequences:

\\" - the quote character (e.g. "String with \\"Quotes\\"")
 \\\\ - the backslash character (e.g. "C:\\\\tmp")

Escape sequences always start with a backslash. Additional escape
sequences might be added in the future. Backslash characters **must** be
escaped. Any other sequence then those outlined above is invalid and may
lead to unpredictable results.

Probably, "msg" is the most prominent use case of property based
filters. It is the actual message text. If you would like to filter
based on some message content (e.g. the presence of a specific code),
this can be done easily by:

``:msg, contains, "ID-4711"``

This filter will match when the message contains the string "ID-4711".
Please note that the comparison is case-sensitive, so it would not match
if "id-4711" would be contained in the message.

``:msg, regex, "fatal .* error"``

This filter uses a POSIX regular expression. It matches when the string
contains the words "fatal" and "error" with anything in between (e.g.
"fatal net error" and "fatal lib error" but not "fatal error" as two
spaces are required by the regular expression!).

Getting property-based filters right can sometimes be challenging. In
order to help you do it with as minimal effort as possible, rsyslogd
spits out debug information for all property-based filters during their
evaluation. To enable this, run rsyslogd in foreground and specify the
"-d" option.

Boolean operations inside property based filters (like 'message contains
"ID17" or message contains "ID18"') are currently not supported (except
for "not" as outlined above). Please note that while it is possible to
query facility and severity via property-based filters, it is far more
advisable to use classic selectors (see above) for those cases.

Expression-Based Filters
------------------------

Expression based filters allow filtering on arbitrary complex
expressions, which can include boolean, arithmetic and string
operations. Expression filters will evolve into a full configuration
scripting language. Unfortunately, their syntax will slightly change
during that process. So if you use them now, you need to be prepared to
change your configuration files some time later. However, we try to
implement the scripting facility as soon as possible (also in respect to
stage work needed). So the window of exposure is probably not too long.

Expression based filters are indicated by the keyword "if" in column 1
of a new line. They have this format:

::

  if expr then action-part-of-selector-line

"if" and "then" are fixed keywords that must be present. "expr" is a
(potentially quite complex) expression. So the :doc:`expression
documentation <../06_reference/rainerscript/expressions>` for details.
"action-part-of-selector-line" is an action, just as you know it (e.g.
"/var/log/logfile" to write to that file).

BSD-style Blocks
----------------

**Note:** rsyslog v7+ no longer supports BSD-style blocks
for technical reasons. So it is strongly recommended **not** to
use them.

Rsyslogd supports BSD-style blocks inside rsyslog.conf. Each block of
lines is separated from the previous block by a program or hostname
specification. A block will only log messages corresponding to the most
recent program and hostname specifications given. Thus, a block which
selects ‘ppp’ as the program, directly followed by a block that selects
messages from the hostname ‘dialhost’, then the second block will only
log messages from the ppp program on dialhost.

A program specification is a line beginning with ‘!prog’ and the
following blocks will be associated with calls to syslog from that
specific program. A program specification for ‘foo’ will also match any
message logged by the kernel with the prefix ‘foo: ’. Alternatively, a
program specification ‘-foo’ causes the following blocks to be applied
to messages from any program but the one specified. A hostname
specification of the form ‘+hostname’ and the following blocks will be
applied to messages received from the specified hostname. Alternatively,
a hostname specification ‘-hostname’ causes the following blocks to be
applied to messages from any host but the one specified. If the hostname
is given as ‘@’, the local hostname will be used. (NOT YET IMPLEMENTED)
A program or hostname specification may be reset by giving the program
or hostname as ‘\*’.

Please note that the "#!prog", "#+hostname" and "#-hostname" syntax
available in BSD syslogd is not supported by rsyslogd. By default, no
hostname or program is set.

Examples
--------

::

  *.* /var/log/file1 # the traditional way
  if $msg contains 'error' then /var/log/errlog # the expression-based way

Right now, you need to specify numerical values if you would like to
check for facilities and severity. These can be found in :rfc:`5424`.
If you don't like that,
you can of course also use the textual property - just be sure to use
the right one. As expression support is enhanced, this will change. For
example, if you would like to filter on message that have facility
local0, start with "DEVNAME" and have either "error1" or "error0" in
their message content, you could use the following filter:

::

  if $syslogfacility-text == 'local0' and $msg startswith 'DEVNAME' and ($msg contains 'error1' or $msg contains 'error0') then /var/log/somelog

Please note that the above must all be on one line! And if you would
like to store all messages except those that contain "error1" or
"error0", you just need to add a "not":

::

  if $syslogfacility-text == 'local0' and $msg startswith 'DEVNAME' and not ($msg contains 'error1' or $msg contains 'error0') then /var/log/somelog

If you would like to do case-insensitive comparisons, use "contains\_i"
instead of "contains" and "startswith\_i" instead of "startswith".
Note that regular expressions are currently NOT supported in
expression-based filters. These will be added later when function
support is added to the expression engine (the reason is that regular
expressions will be a separate loadable module, which requires some more
prerequisites before it can be implemented).


This is a part of the rsyslog.conf - documentation.

`back <rsyslog_conf.html>`_

Templates
---------

Templates are a key feature of rsyslog. They allow to specify any format
a user might want. They are also used for dynamic file name generation.
Every output in rsyslog uses templates - this holds true for files, user
messages and so on. The database writer expects its template to be a
proper SQL statement - so this is highly customizable too. You might ask
how does all of this work when no templates at all are specified. Good
question ;) The answer is simple, though. Templates compatible with the
stock syslogd formats are hardcoded into rsyslogd. So if no template is
specified, we use one of these hardcoded templates. Search for
"template\_" in syslogd.c and you will find the hardcoded ones.

Starting with 5.5.6, there are actually two differnt types of template:

-  string based
-  string-generator module based

`String-generator module <rsyslog_conf_modules.html#sm>`_ based
templates have been introduced in 5.5.6. They permit a string generator,
actually a C "program", the generate a format. Obviously, it is more
work required to code such a generator, but the reward is speed
improvement. If you do not need the ultimate throughput, you can forget
about string generators (so most people never need to know what they
are). You may just be interested in learning that for the most important
default formats, rsyslog already contains highly optimized string
generators and these are called without any need to configure anything.
But if you have written (or purchased) a string generator module, you
need to know how to call it. Each such module has a name, which you need
to know (look it up in the module doc or ask the developer). Let's
assume that "mystrgen" is the module name. Then you can define a
template for that strgen in the following way:

    ``$template MyTemplateName,=mystrgen``

(Of course, you must have first loaded the module via $ModLoad).

The important part is the equal sign: it tells the rsyslog config parser
that no string follows but a strgen module name.

There are no additional parameters but the module name supported. This
is because there is no way to customize anything inside such a
"template" other than by modifying the code of the string generator.

So for most use cases, string-generator module based templates are
**not** the route to take. Usually, us use **string based templates**
instead. This is what the rest of the documentation now talks about.

A template consists of a template directive, a name, the actual template
text and optional options. A sample is:

    ``$template MyTemplateName,"\7Text %property% some more text\n",<options>``

The "$template" is the template directive. It tells rsyslog that this
line contains a template. "MyTemplateName" is the template name. All
other config lines refer to this name. The text within quotes is the
actual template text. The backslash is an escape character, much as it
is in C. It does all these "cool" things. For example, \\7 rings the
bell (this is an ASCII value), \\n is a new line. C programmers and perl
coders have the advantage of knowing this, but the set in rsyslog is a
bit restricted currently.

All text in the template is used literally, except for things within
percent signs. These are properties and allow you access to the contents
of the syslog message. Properties are accessed via the `property
replacer <property_replacer.html>`_ (nice name, huh) and it can do cool
things, too. For example, it can pick a substring or do date-specific
formatting. More on this is below, on some lines of the property
replacer.

The <options> part is optional. It carries options influencing the
template as whole. See details below. Be sure NOT to mistake template
options with property options - the later ones are processed by the
property replacer and apply to a SINGLE property, only (and not the
whole template).

Template options are case-insensitive. Currently defined are:

**sql** - format the string suitable for a SQL statement in MySQL
format. This will replace single quotes ("'") and the backslash
character by their backslash-escaped counterpart ("\\'" and "\\\\")
inside each field. Please note that in MySQL configuration, the
``NO_BACKSLASH_ESCAPES`` mode must be turned off for this format to work
(this is the default).

**stdsql** - format the string suitable for a SQL statement that is to
be sent to a standards-compliant sql server. This will replace single
quotes ("'") by two single quotes ("''") inside each field. You must use
stdsql together with MySQL if in MySQL configuration the
``NO_BACKSLASH_ESCAPES`` is turned on.

Either the **sql** or **stdsql**  option **must** be specified when a
template is used for writing to a database, otherwise injection might
occur. Please note that due to the unfortunate fact that several vendors
have violated the sql standard and introduced their own escape methods,
it is impossible to have a single option doing all the work.  So you
yourself must make sure you are using the right format. **If you choose
the wrong one, you are still vulnerable to sql injection.**

Please note that the database writer \*checks\* that the sql option is
present in the template. If it is not present, the write database action
is disabled. This is to guard you against accidental forgetting it and
then becoming vulnerable to SQL injection. The sql option can also be
useful with files - especially if you want to import them into a
database on another machine for performance reasons. However, do NOT use
it if you do not have a real need for it - among others, it takes some
toll on the processing time. Not much, but on a really busy system you
might notice it ;)

The default template for the write to database action has the sql option
set. As we currently support only MySQL and the sql option matches the
default MySQL configuration, this is a good choice. However, if you have
turned on ``NO_BACKSLASH_ESCAPES`` in your MySQL config, you need to
supply a template with the stdsql option. Otherwise you will become
vulnerable to SQL injection.

Escaping is done as in C.  For example to escape the percent symbol:

``\%``

To escape a backslash use another backslash:

``\\``

``$template TraditionalFormat,"%timegenerated% %HOSTNAME% %syslogtag%%msg%\\n"``

Properties can be accessed by the `property replacer <property_replacer.html>`_ (see there for details).

**Please note that templates can also by used to generate selector lines
with dynamic file names.** For example, if you would like to split
syslog messages from different hosts to different files (one per host),
you can define the following template:

    ``$template DynFile,"/var/log/system-%HOSTNAME%.log"``

This template can then be used when defining an output selector line. It
will result in something like "/var/log/system-localhost.log"

Template names beginning with "RSYSLOG\_" are reserved for rsyslog use.
Do NOT use them if, otherwise you may receive a conflict in the future
(and quite unpredictable behaviour). There is a small set of pre-defined
templates that you can use without the need to define it:

-  RSYSLOG\_TraditionalFileFormat - the "old style" default log file
   format with low-precision timestamps
-  RSYSLOG\_FileFormat - a modern-style logfile format similar to
   TraditionalFileFormat, buth with high-precision timestamps and
   timezone information
-  RSYSLOG\_TraditionalForwardFormat - the traditional forwarding format
   with low-precision timestamps. Most useful if you send messages to
   other syslogd's or rsyslogd below version 3.12.5.
-  RSYSLOG\_SysklogdFileFormat - sysklogd compatible log file format. If
   used with options: $SpaceLFOnReceive on;
   $EscapeControlCharactersOnReceive off; $DropTrailingLFOnReception
   off, the log format will conform to sysklogd log format.
-  RSYSLOG\_ForwardFormat - a new high-precision forwarding format very
   similar to the traditional one, but with high-precision timestamps
   and timezone information. Recommended to be used when sending
   messages to rsyslog 3.12.5 or above.
-  RSYSLOG\_SyslogProtocol23Format - the format specified in IETF's
   internet-draft ietf-syslog-protocol-23, which is assumed to be come
   the new syslog standard RFC. This format includes several
   improvements. The rsyslog message parser understands this format, so
   you can use it together with all relatively recent versions of
   rsyslog. Other syslogd's may get hopelessly confused if receiving
   that format, so check before you use it. Note that the format is
   unlikely to change when the final RFC comes out, but this may happen.
-  RSYSLOG\_DebugFormat - a special format used for troubleshooting
   property problems. This format is meant to be written to a log file.
   Do **not** use for production or remote forwarding.

String-based Template Samples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section provides some sample of what the default formats would look
as a text-based template. Hopefully, their description is
self-explanatory. Note that each $Template statement is on a **single**
line, but probably broken accross several lines for display purposes by
your browsers. Lines are separated by empty lines.

``$template FileFormat,"%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"``

``$template TraditionalFileFormat,"%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"``

``$template ForwardFormat,"<%PRI%>%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%"``

``$template TraditionalForwardFormat,"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%"``

``$template StdSQLFormat,"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')",SQL``

[`manual index <manual.html>`_\ ]
[`rsyslog.conf <rsyslog_conf.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_ project.

Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.

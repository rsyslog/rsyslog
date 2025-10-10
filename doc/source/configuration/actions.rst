Actions
=======

.. index:: ! action
.. _cfgobj_input:

The Action object describes what is to be done with a message. They are
implemented via :doc:`output modules <modules/idx_output>`.

The action object has different parameters:

-  those that apply to all actions and are action specific. These are
   documented below.

-  parameters for the action queue. While they also apply to all
   parameters, they are queue-specific, not action-specific (they are
   the same that are used in rulesets, for example). They are documented
   separately under :doc:`queue parameters <../rainerscript/queue_parameters>`.

-  action-specific parameters. These are specific to a certain type of
   actions. They are documented by the :doc:`output modules<modules/idx_output>`
   in question.

General Action Parameters
-------------------------

Note: parameter names are case-insensitive.

-  **name** word

   This names the action. The name is used for statistics gathering
   and documentation. If no name is given, one is dynamically generated
   based on the occurrence of this action inside the rsyslog configuration.
   Actions are sequentially numbered from 1 to n.

-  **type** string

   Mandatory parameter for every action. The name of the module that
   should be used.

-  **action.writeAllMarkMessages** *on*/off

   This setting tells if mark messages are always written ("on", the
   default) or only if the action was not recently executed ("off"). By
   default, recently means within the past 20 minutes. If this setting
   is "on", mark messages are always sent to actions, no matter how
   recently they have been executed. In this mode, mark messages can be
   used as a kind of heartbeat. This mode also enables faster processing
   inside the rule engine. So it should be set to "off" only when there
   is a good reason to do so.

-  **action.execOnlyEveryNthTime** integer

   If configured, the next action will only be executed every n-th time.
   For example, if configured to 3, the first two messages that go into
   the action will be dropped, the 3rd will actually cause the action to
   execute, the 4th and 5th will be dropped, the 6th executed under the
   action, ... and so on.

-  **action.execOnlyEveryNthTimeout** integer

   Has a meaning only if Action.ExecOnlyEveryNthTime is also configured
   for the same action. If so, the timeout setting specifies after which
   period the counting of "previous actions" expires and a new action
   count is begun. Specify 0 (the default) to disable timeouts. Why is
   this option needed? Consider this case: a message comes in at, eg.,
   10am. That's count 1. Then, nothing happens for the next 10 hours. At
   8pm, the next one occurs. That's count 2. Another 5 hours later, the
   next message occurs, bringing the total count to 3. Thus, this
   message now triggers the rule. The question is if this is desired
   behavior? Or should the rule only be triggered if the messages occur
   within an e.g. 20 minute window? If the later is the case, you need a
   Action.ExecOnlyEveryNthTimeTimeout="1200"
   This directive will timeout previous messages seen if they are older
   than 20 minutes. In the example above, the count would now be always
   1 and consequently no rule would ever be triggered.

-  **action.errorfile** string

   .. versionadded:: 8.32.0

   When an action is executed, some messages may permanently fail.
   Depending on configuration, this could for example be caused by an
   offline target or exceptionally non-numerical data inside a
   numerical database field. If action.errorfile is specified, those
   messages are written to the specified file. If it is not specified
   (the default), messages are silently discarded.

   The error file format is JSON. It contains the failed messages as
   provided to the action in question, the action name as well as
   the rsyslog status code roughly explaining why it failed.

-  **action.errorfile.maxsize** integer

   In some cases, error file needs to be limited in size.
   This option allows specifying a maximum size, in bytes, for the error file.
   When error file reaches that size, no more errors are written to it.

-  **action.execOnlyOnceEveryInterval** integer

   Execute action only if the last execute is at last seconds in the
   past (more info in ommail, but may be used with any action)

-  **action.execOnlyWhenPreviousIsSuspended** on/off

   This directive allows to specify if actions should always be executed
   ("off," the default) or only if the previous action is suspended
   ("on"). This directive works hand-in-hand with the multiple actions
   per selector feature. It can be used, for example, to create rules
   that automatically switch destination servers or databases to a (set
   of) backup(s), if the primary server fails. Note that this feature
   depends on proper implementation of the suspend feature in the output
   module. All built-in output modules properly support it (most
   importantly the database write and the syslog message forwarder).
   Note, however, that a failed action may not immediately be detected.
   For more information, see the `rsyslog
   execOnlyWhenPreviousIsSuspended
   preciseness <https://www.rsyslog.com/action-execonlywhenpreviousissuspended-preciseness/>`_
   FAQ article.

-  **action.repeatedmsgcontainsoriginalmsg** on/off

   "last message repeated n times" messages, if generated, have a
   different format that contains the message that is being repeated.
   Note that only the first "n" characters are included, with n to be at
   least 80 characters, most probably more (this may change from version
   to version, thus no specific limit is given). The bottom line is that
   n is large enough to get a good idea which message was repeated but
   it is not necessarily large enough for the whole message. (Introduced
   with 4.1.5).

-  **action.resumeRetryCount** integer

   [default 0, -1 means eternal]

   Sets how often an action is retried before it is considered to have
   failed. Failed actions discard messages.

-  **action.resumeInterval** integer

   Sets the action's resume interval. The interval provided
   is always in seconds. Thus, multiply by 60 if you need minutes and
   3,600 if you need hours (not recommended). When an action is
   suspended (e.g. destination can not be connected), the action is
   resumed for the configured interval. Thereafter, it is retried. If
   multiple retries fail, the interval is automatically extended. This
   is to prevent excessive resource use for retries. After each 10
   retries, the interval is extended by itself. To be precise, the 
   actual interval is `(numRetries / 10 + 1) * action.resumeInterval`.
   Using the default value of 30, this means that on the 10th try the
   suspension interval will be 60 (seconds) and after the 100th try
   it will be 330 (seconds).

-  **action.resumeIntervalMax** integer

   Default: 1800 (30 minutes)

   This sets an upper limit on the growth of action.resumeInterval.
   No wait will be larger than the value configured here. Going higher
   than the default is only recommended if you know that a system may
   be offline for an extended period of time **and** if it is acceptable
   that it may take quite long to detect it came online again.

- **action.reportSuspension** on/off

  Configures rsyslog to report suspension and reactivation
  of the action. This is useful to note which actions have
  problems (e.g. connecting to a remote system) and when.
  The default for this setting is the equally-named global
  parameter.

- **action.reportSuspensionContinuation** on/off

  Configures rsyslog to report continuation of action suspension.
  This emits new messages whenever an action is to be retried, but
  continues to fail. If set to "on", *action.reportSuspension* is
  also automatically set to "on".
  The default for this setting is the equally-named global
  parameter.

- **action.copyMsg** on/*off*

  Configures action to *copy* the message if *on*. Defaults to
  *off* (which is how actions have worked traditionally), which
  causes queue to refer to the original message object, with
  reference-counting. (Introduced with 8.10.0).

Useful Links
------------

-  Rainer's blog posting on the performance of `main and action queue
   worker
   threads <https://rainer.gerhards.net/2013/06/rsyslog-performance-main-and-action.html>`_

Legacy Format
-------------

.. _legacy-action-order:

**Be warned that legacy action format is hard to get right. It is
recommended to use RainerScript-Style action format whenever possible!**
A key problem with legacy format is that a single action is defined via
multiple configurations lines, which may be spread all across
rsyslog.conf. Even the definition of multiple actions may be intermixed
(often not intentional!). If legacy actions format needs to be used
(e.g. some modules may not yet implement the RainerScript format), it is
strongly recommended to place all configuration statements pertaining to
a single action closely together.

Please also note that legacy action parameters **do not** affect
RainerScript action objects. So if you define for example:

::

    $actionResumeRetryCount 10
    action(type="omfwd" target="server1.example.net")
    @@server2.example.net

server1's "action.resumeRetryCount" parameter is **not** set, instead
server2's is!

A goal of the new RainerScript action format was to avoid confusion
which parameters are actually used. As such, it would be
counter-productive to honor legacy action parameters inside a
RainerScript definition. As result, both types of action definitions are
strictly (and nicely) separated from each other. The bottom line is that
if RainerScript actions are used, one does not need to care about which
legacy action parameters may (still...) be in effect.

Note that not all modules necessarily support legacy action format.
Especially newer modules are recommended to NOT support it.

Legacy Description
~~~~~~~~~~~~~~~~~~

Templates can be used with many actions. If used, the specified template
is used to generate the message content (instead of the default
template). To specify a template, write a semicolon after the action
value immediately followed by the template name.
Beware: templates MUST be defined BEFORE they are used. It is OK to
define some templates, then use them in selector lines, define more
templates and use them in the following selector lines. But it is
NOT permitted to use a template in a selector line that is above its
definition. If you do this, the action will be ignored.

**You can have multiple actions for a single selector** (or more
precisely a single filter of such a selector line). Each action must be
on its own line and the line must start with an ampersand ('&')
character and have no filters. An example would be

::

  *.=crit :omusrmsg:rger
  & root
  & /var/log/critmsgs

These three lines send critical messages to the user rger and root and
also store them in /var/log/critmsgs. **Using multiple actions per
selector is** convenient and also **offers a performance benefit**. As
the filter needs to be evaluated only once, there is less computation
required to process the directive compared to the otherwise-equal config
directives below:

::

  *.=crit :omusrmsg:rger
  *.=crit root
  *.=crit /var/log/critmsgs

Regular File
~~~~~~~~~~~~

Typically messages are logged to real files. The file usually is
specified by full pathname, beginning with a slash "/". Starting with
version 4.6.2 and 5.4.1 (previous v5 version do NOT support this)
relative file names can also be specified. To do so, these must begin
with a dot. For example, use "./file-in-current-dir.log" to specify a
file in the current directory. Please note that rsyslogd usually changes
its working directory to the root, so relative file names must be tested
with care (they were introduced primarily as a debugging vehicle, but
may have useful other applications as well).
You may prefix each entry with the minus "-'' sign to omit syncing the
file after every logging. Note that you might lose information if the
system crashes right behind a write attempt. Nevertheless this might
give you back some performance, especially if you run programs that use
logging in a very verbose manner.

If your system is connected to a reliable UPS and you receive lots of
log data (e.g. firewall logs), it might be a very good idea to turn of
syncing by specifying the "-" in front of the file name.

**The filename can be either static**\ (always the same) or **dynamic**
(different based on message received). The later is useful if you would
automatically split messages into different files based on some message
criteria. For example, dynamic file name selectors allow you to split
messages into different files based on the host that sent them. With
dynamic file names, everything is automatic and you do not need any
filters.

It works via the template system. First, you define a template for the
file name. An example can be seen above in the description of template.
We will use the "DynFile" template defined there. Dynamic filenames are
indicated by specifying a questions mark "?" instead of a slash,
followed by the template name. Thus, the selector line for our dynamic
file name would look as follows:

    ``*.* ?DynFile``

That's all you need to do. Rsyslog will now automatically generate file
names for you and store the right messages into the right files. Please
note that the minus sign also works with dynamic file name selectors.
Thus, to avoid syncing, you may use

    ``*.* -?DynFile``

And of course you can use templates to specify the output format:

    ``*.* ?DynFile;MyTemplate``

**A word of caution:** rsyslog creates files as needed. So if a new host
is using your syslog server, rsyslog will automatically create a new
file for it.

**Creating directories is also supported**. For example you can use the
hostname as directory and the program name as file name:

    ``$template DynFile,"/var/log/%HOSTNAME%/%programname%.log"``

Named Pipes
~~~~~~~~~~~

This version of rsyslogd(8) has support for logging output to named
pipes (fifos). A fifo or named pipe can be used as a destination for log
messages by prepending a pipe symbol ("\|'') to the name of the file.
This is handy for debugging. Note that the fifo must be created with the
mkfifo(1) command before rsyslogd(8) is started.

Terminal and Console
~~~~~~~~~~~~~~~~~~~~

If the file you specified is a tty, special tty-handling is done, same
with /dev/console.

Remote Machine
~~~~~~~~~~~~~~

Rsyslogd provides full remote logging, i.e. is able to send messages to
a remote host running rsyslogd(8) and to receive messages from remote
hosts. Using this feature you're able to control all syslog messages on
one host, if all other machines will log remotely to that. This tears
down administration needs.

To forward messages to another host, prepend the hostname with the at
sign ("@"). A single at sign means that messages will be forwarded via
UDP protocol (the standard for syslog). If you prepend two at signs
("@@"), the messages will be transmitted via TCP. Please note that plain
TCP based syslog is not officially standardized, but most major syslogds
support it (e.g. syslog-ng or `WinSyslog <https://www.winsyslog.com/>`_).
The forwarding action indicator (at-sign) can be followed by one or more
options. If they are given, they must be immediately (without a space)
following the final at sign and be enclosed in parenthesis. The
individual options must be separated by commas. The following options
are right now defined:

**z<number>**

Enable zlib-compression for the message. The <number> is the compression
level. It can be 1 (lowest gain, lowest CPU overhead) to 9 (maximum
compression, highest CPU overhead). The level can also be 0, which means
"no compression". If given, the "z" option is ignored. So this does not
make an awful lot of sense. There is hardly a difference between level 1
and 9 for typical syslog messages. You can expect a compression gain
between 0% and 30% for typical messages. Very chatty messages may
compress up to 50%, but this is seldom seen with typically traffic.
Please note that rsyslogd checks the compression gain. Messages with 60
bytes or less will never be compressed. This is because compression gain
is pretty unlikely and we prefer to save CPU cycles. Messages over that
size are always compressed. However, it is checked if there is a gain in
compression and only if there is, the compressed message is transmitted.
Otherwise, the uncompressed messages is transmitted. This saves the
receiver CPU cycles for decompression. It also prevents small message to
actually become larger in compressed form.

**Please note that when a TCP transport is used, compression will also
turn on syslog-transport-tls framing. See the "o" option for important
information on the implications.**

Compressed messages are automatically detected and decompressed by the
receiver. There is nothing that needs to be configured on the receiver
side.

**o**

**This option is experimental. Use at your own risk and only if you know
why you need it! If in doubt, do NOT turn it on.**

This option is only valid for plain TCP based transports. It selects a
different framing based on IETF internet draft syslog-transport-tls-06.
This framing offers some benefits over traditional LF-based framing.
However, the standardization effort is not yet complete. There may be
changes in upcoming versions of this standard. Rsyslog will be kept in
line with the standard. There is some chance that upcoming changes will
be incompatible to the current specification. In this case, all systems
using -transport-tls framing must be upgraded. There will be no effort
made to retain compatibility between different versions of rsyslog. The
primary reason for that is that it seems technically impossible to
provide compatibility between some of those changes. So you should take
this note very serious. It is not something we do not \*like\* to do
(and may change our mind if enough people beg...), it is something we
most probably \*can not\* do for technical reasons (aka: you can beg as
much as you like, it won't change anything...).

The most important implication is that compressed syslog messages via
TCP must be considered with care. Unfortunately, it is technically
impossible to transfer compressed records over traditional syslog plain
tcp transports, so you are left with two evil choices...

 The hostname may be followed by a colon and the destination port.

The following is an example selector line with forwarding:

\*.\*    @@(o,z9)192.168.0.1:1470

In this example, messages are forwarded via plain TCP with experimental
framing and maximum compression to the host 192.168.0.1 at port 1470.

\*.\* @192.168.0.1

In the example above, messages are forwarded via UDP to the machine
192.168.0.1, the destination port defaults to 514. Messages will not be
compressed.

Note that IPv6 addresses contain colons. So if an IPv6 address is
specified in the hostname part, rsyslogd could not detect where the IP
address ends and where the port starts. There is a syntax extension to
support this: put square brackets around the address (e.g. "[2001::1]").
Square brackets also work with real host names and IPv4 addresses, too.

A valid sample to send messages to the IPv6 host 2001::1 at port 515 is
as follows:

\*.\* @[2001::1]:515

This works with TCP, too.

**Note to sysklogd users:** sysklogd does **not** support RFC 3164
format, which is the default forwarding template in rsyslog. As such,
you will experience duplicate hostnames if rsyslog is the sender and
sysklogd is the receiver. The fix is simple: you need to use a different
template. Use that one:

$template sysklogd,"<%PRI%>%TIMESTAMP% %syslogtag%%msg%\\""
 \*.\* @192.168.0.1;sysklogd

List of Users
~~~~~~~~~~~~~

Usually critical messages are also directed to "root'' on that machine.
You can specify a list of users that shall get the message by simply
writing ":omusrmsg: followed by the login name. For example, the send
messages to root, use ":omusrmsg:root". You may specify more than one
user by separating them with commas (",''). Do not repeat the
":omusrmsg:" prefix in this case. For example, to send data to users
root and rger, use ":omusrmsg:root,rger" (do not use
":omusrmsg:root,:omusrmsg:rger", this is invalid). If they're logged in
they get the message.

Everyone logged on
~~~~~~~~~~~~~~~~~~

Emergency messages often go to all users currently online to notify them
that something strange is happening with the system. To specify this
wall(1)-feature use an asterisk as the user message
destination(":omusrmsg:\*'').

Call Plugin
~~~~~~~~~~~

This is a generic way to call an output plugin. The plugin must support
this functionality. Actual parameters depend on the module, so see the
module's doc on what to supply. The general syntax is as follows:

:modname:params;template

Currently, the ommysql database output module supports this syntax (in
addition to the ">" syntax it traditionally supported). For ommysql, the
module name is "ommysql" and the params are the traditional ones. The
;template part is not module specific, it is generic rsyslog
functionality available to all modules.

As an example, the ommysql module may be called as follows:

:ommysql:dbhost,dbname,dbuser,dbpassword;dbtemplate

For details, please see the "Database Table" section of this
documentation.

Note: as of this writing, the ":modname:" part is hardcoded into the
module. So the name to use is not necessarily the name the module's
plugin file is called.

Database Table
~~~~~~~~~~~~~~

This allows logging of the message to a database table. Currently, only
MariaDB/MySQL databases are supported. However, other database drivers 
will most probably be developed as plugins. By default, a
`MonitorWare <https://www.mwagent.com/>`_-compatible schema is
required for this to work. You can create that schema with the
createDB.SQL file that came with the rsyslog package. You can also
use any other schema of your liking - you just need to define a proper
template and assign this template to the action.
The database writer is called by specifying a greater-then sign (">")
in front of the database connect information. Immediately after that
sign the database host name must be given, a comma, the database name,
another comma, the database user, a comma and then the user's password.
If a specific template is to be used, a semicolon followed by the
template name can follow the connect information. This is as follows:
>dbhost,dbname,dbuser,dbpassword;dbtemplate

**Important: to use the database functionality, the MariaDB/MySQL output 
module must be loaded in the config file** BEFORE the first database table
action is used. This is done by placing the

::

  $ModLoad ommysql

directive some place above the first use of the database write (we
recommend doing at the beginning of the config file).

Discard / Stop
~~~~~~~~~~~~~~

If the discard action is carried out, the received message is
immediately discarded. No further processing of it occurs. Discard has
primarily been added to filter out messages before carrying on any
further processing. For obvious reasons, the results of "discard" are
depending on where in the configuration file it is being used. Please
note that once a message has been discarded there is no way to retrieve
it in later configuration file lines.

Discard can be highly effective if you want to filter out some annoying
messages that otherwise would fill your log files. To do that, place the
discard actions early in your log files. This often plays well with
property-based filters, giving you great freedom in specifying what you
do not want.

Discard is just the word "stop" with no further parameters:

stop

For example,

\*.\*   stop

discards everything (ok, you can achieve the same by not running rsyslogd
at all...).

Note that in legacy configuration the tilde character "~" can also be
used instead of the word "stop". This is nowadays a very bad practice and
should be avoided.

Output Channel
~~~~~~~~~~~~~~

Binds an output channel definition (see there for details) to this
action. Output channel actions must start with a $-sign, e.g. if you
would like to bind your output channel definition "mychannel" to the
action, use "$mychannel". Output channels support template definitions
like all other actions.

Shell Execute
~~~~~~~~~~~~~

**NOTE: This action is only supported for backwards compatibility.
For new configs, use** :doc:`omprog <modules/omprog>` **instead.
It provides a more solid
and secure solution with higher performance.**

This executes a program in a subshell. The program is passed the
template-generated message as the only command line parameter. Rsyslog
waits until the program terminates and only then continues to run.

^program-to-execute;template

The program-to-execute can be any valid executable. It receives the
template string as a single parameter (argv[1]).

**WARNING:** The Shell Execute action was added to serve an urgent need.
While it is considered reasonable save when used with some thinking, its
implications must be considered. The current implementation uses a
system() call to execute the command. This is not the best way to do it
(and will hopefully changed in further releases). Also, proper escaping
of special characters is done to prevent command injection. However,
attackers always find smart ways to circumvent escaping, so we can not
say if the escaping applied will really safe you from all hassles.
Lastly, rsyslog will wait until the shell command terminates. Thus, a
program error in it (e.g. an infinite loop) can actually disable
rsyslog. Even without that, during the programs run-time no messages are
processed by rsyslog. As the IP stacks buffers are quickly overflowed,
this bears an increased risk of message loss. You must be aware of these
implications. Even though they are severe, there are several cases where
the "shell execute" action is very useful. This is the reason why we
have included it in its current form. To mitigate its risks, always a)
test your program thoroughly, b) make sure its runtime is as short as
possible (if it requires a longer run-time, you might want to spawn your
own sub-shell asynchronously), c) apply proper firewalling so that only
known senders can send syslog messages to rsyslog. Point c) is
especially important: if rsyslog is accepting message from any hosts,
chances are much higher that an attacker might try to exploit the "shell
execute" action.

Template Name
~~~~~~~~~~~~~

Every ACTION can be followed by a template name. If so, that template is
used for message formatting. If no name is given, a hard-coded default
template is used for the action. There can only be one template name for
each given action. The default template is specific to each action. For
a description of what a template is and what you can do with it, see the
:doc:`template<templates>` documentation.

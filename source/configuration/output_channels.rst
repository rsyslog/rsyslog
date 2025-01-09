Output Channels
---------------

Output Channels are a new concept first introduced in rsyslog 0.9.0.
**As of this writing, it is most likely that they will be replaced by
something different in the future.** So if you use them, be prepared to
change you configuration file syntax when you upgrade to a later
release.
The idea behind output channel definitions is that it shall provide an
umbrella for any type of output that the user might want. In essence,
this is the "file" part of selector lines (and this is why we are not
sure output channel syntax will stay after the next review). There is a
difference, though: selector channels both have filter conditions
(currently facility and severity) as well as the output destination.
they can only be used to write to files - not pipes, ttys or whatever
Output channels define the output definition, only. As of this build,
else. If we stick with output channels, this will change over time.

In concept, an output channel includes everything needed to know about
an output actions. In practice, the current implementation only carries
a filename, a maximum file size and a command to be issued when this
file size is reached. More things might be present in future version,
which might also change the syntax of the directive.

Output channels are defined via an $outchannel directive. It's syntax is
as follows:
$outchannel name,file-name,max-size,action-on-max-size
name is the name of the output channel (not the file), file-name is the
file name to be written to, max-size the maximum allowed size and
action-on-max-size a command to be issued when the max size is reached.
This command always has exactly one parameter. The binary is that part
of action-on-max-size before the first space, its parameter is
everything behind that space.
Please note that max-size is queried BEFORE writing the log message to
the file. So be sure to set this limit reasonably low so that any
message might fit. For the current release, setting it 1k lower than you
expected is helpful. The max-size must always be specified in bytes -
there are no special symbols (like 1k, 1m,...) at this point of
development.
Keep in mind that $outchannel just defines a channel with "name". It
does not activate it. To do so, you must use a selector line (see
below). That selector line includes the channel name plus a $ sign in
front of it. A sample might be:
\*.\* :omfile:$mychannel
In its current form, output channels primarily provide the ability to
size-limit an output file. To do so, specify a maximum size. When this
size is reached, rsyslogd will execute the action-on-max-size command
and then reopen the file and retry. The command should be something like
a `log rotation script <log_rotation_fix_size.html>`_ or a similar
thing.

If there is no action-on-max-size command or the command did not resolve
the situation, the file is closed and never reopened by rsyslogd
(except, of course, by huping it). This logic was integrated when we
first experienced severe issues with files larger 2gb, which could lead
to rsyslogd dumping core. In such cases, it is more appropriate to stop
writing to a single file. Meanwhile, rsyslogd has been fixed to support
files larger 2gb, but obviously only on file systems and operating
system versions that do so. So it can still make sense to enforce a 2gb
file size limit.


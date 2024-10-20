Compatibility Notes for rsyslog v4
==================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_
*(2009-07-15)*

The changes introduced in rsyslog v4 are numerous, but not very
intrusive. This document describes things to keep in mind when moving
from v3 to v4. It does not list enhancements nor does it talk about
compatibility concerns introduced by v3 (for this, see the `rsyslog v3
compatibility notes <v3compatibility.html>`_).

HUP processing
--------------

With v3 and below, rsyslog used the traditional HUP behaviour. That
meant that all output files are closed and the configuration file is
re-read and the new configuration applied.

With a program as simple and static as sysklogd, this was not much of an
issue. The most important config settings (like udp reception) of a
traditional syslogd can not be modified via the configuration file. So a
config file reload only meant setting up a new set of filters. It also
didn't account as problem that while doing so messages may be lost -
without any threading and queuing model, a traditional syslogd will
potentially always loose messages, so it is irrelevant if this happens,
too, during the short config re-read phase.

In rsyslog, things are quite different: the program is more or less a
framework into which loadable modules are loaded as needed for a
particular configuration. The software that will actually be running is
tailored via the config file. Thus, a re-read of the config file
requires a full, very heavy restart, because the software actually
running with the new config can be totally different from what ran with
the old config.

Consequently, the traditional HUP is a very heavy operation and may even
cause some data loss because queues must be shut down, listeners stopped
and so on. Some of these operations (depending on their configuration)
involve intentional message loss. The operation also takes up a lot of
system resources and needs quite some time (maybe seconds) to be
completed. During this restart period, the syslog subsystem is not fully
available.

From the software developer's point of view, the full restart done by a
HUP is rather complex, especially if user-timeout limits set on action
completion are taken into consideration (for those in the know: at the
extreme ends this means we need to cancel threads as a last resort, but
than we need to make sure that such cancellation does not happen at
points where it would be fatal for a restart). A regular restart, where
the process is actually terminated, is much less complex, because the
operating system does a full cleanup after process termination, so
rsyslogd does not need to take care for exotic cleanup cases and leave
that to the OS. In the end result, restart-type HUPs clutter the code,
increase complexity (read: add bugs) and cost performance.

On the contrary, a HUP is typically needed for log rotation, and the
real desire is to close files. This is a non-disruptive and very
lightweight operation.

Many people have said that they are used to HUP the syslogd to apply
configuration changes. This is true, but it is questionable if that
really justifies all the cost that comes with it. After all, it is the
difference between typing

::

    $ kill -HUP `cat /var/run/rsyslogd.pid`

versus

::

    $ /etc/init.d/rsyslog restart

Semantically, both is mostly the same thing. The only difference is that
with the restart command rsyslogd can spit config error message to
stderr, so that the user is able to see any problems and fix them. With
a HUP, we do not have access to stderr and thus can log error messages
only to their configured destinations; experience tells that most users
will never find them there. What, by the way, is another strong argument
against restarting rsyslogd by HUPing it.

So a restart via HUP is not strictly necessary and most other daemons
require that a restart command is typed in if a restart is required.

Rsyslog will follow this paradigm in the next versions, resulting in
many benefits. In v4, we provide some support for the old-style
semantics. We introduced a setting $HUPisRestart which may be set to
"on" (traditional, heavy operation) or "off" (new, lightweight "file
close only" operation). The initial versions had the default set to
traditional behavior, but starting with 4.5.1 we are now using the new
behavior as the default.

Most importantly, **this may break some scripts**, but my sincere belief
is that there are very few scripts that automatically **change**
rsyslog's config and then do a HUP to reload it. Anyhow, if you have
some of these, it may be a good idea to change them now instead of
turning restart-type HUPs on. Other than that, one mainly needs to
change the habit of how to restart rsyslog after a configuration change.

**Please note that restart-type HUP is deprecated and will go away in
rsyslog v5.** So it is a good idea to become ready for the new version
now and also enjoy some of the benefits of the "real restart", like the
better error-reporting capability.

Note that code complexity reduction (and thus performance improvement)
needs the restart-type HUP code to be removed, so these changes can (and
will) only happen in version 5.

outchannels
-----------

Note: as always documented, outchannels are an experimental feature that
may be removed and/or changed in the future. There is one concrete
change done starting with 4.6.7: let's assume an outchannel "mychannel"
was defined. Then, this channel could be used inside an

\*.\* $mychannel 

This is still supported and will remain to be
supported in v4. However, there is a new variant which explicitly tells
this is to be handled by omfile. This new syntax is as follows:

\*.\* :omfile:$mychannel 

Note that future versions, specifically
starting with v6, the older syntax is no longer supported. So users are
strongly advised to switch to the new syntax. As an aid to the
conversion process, rsyslog 4.7.4 and above issue a warning message if
the old-style directive is seen -- but still accept the old syntax
without any problems.

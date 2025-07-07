Compatibility Notes for rsyslog v6
==================================

This document describes things to keep in mind when moving from v5 to v6. It 
does not list enhancements nor does it talk about compatibility concerns introduced
by earlier versions (for this, see their respective compatibility documents). Its focus
is primarily on what you need to know if you used a previous version and want to use the
current one without hassle.

Version 6 offers a better config language and some other improvements.
As the config system has many ties into the rsyslog engine AND all plugins,
the changes are somewhat intrusive. Note, however, that core processing has
not been changed much in v6 and will not. So once the configuration is loaded,
the stability of v6 is quite comparable to v5.

Property "pri-text"
-------------------
Traditionally, this property did not only return the textual form
of the pri ("local0.err"), but also appended the numerical value to it
("local0.err<133>"). This sounds odd and was left unnoticed for some years. 
In October 2011, this odd behaviour was brought up on the rsyslog mailing list
by Gregory K. Ruiz-Ade. Code review showed that the behaviour was intentional,
but no trace of what the intention was when it was introduced could be found.
The documentation was also unclear, it said no numerical value was present,
but the samples had it. We agreed that the additional numerical value is
of disadvantage. We also guessed that this property is very rarely being used,
otherwise the problem should have been raised much earlier. However, we 
didn't want to change behaviour in older builds. So v6 was set to clean up
the situation. In v6, text-pri will always return the textual part only
("local0.err") and the numerical value will not be contained any longer inside
the string. If you actually need that value, it can fairly easily be added
via the template system.
**If you have used this property previously and relied on the numerical
part, you need to update your rsyslog configuration files.**

Plugin ABI
----------
The plugin interface has considerably been changed to support the new
config language. All plugins need to be upgraded. This usually does not require
much coding. However, if the new config language shall be supported, more
changes must be made to plugin code. All project-supported plugins have been
upgraded, so this compatibility issue is only of interest for you if you have
custom plugins or use some user-contributed plugins from the rsyslog project
that are not maintained by the project itself (omoracle is an example). Please
expect some further plugin instability during the initial v6 releases.

RainerScript based rsyslog.conf
-------------------------------
A better config format was the main release target for rsyslog v6. It comes in the
flavor of so-called RainerScript
`(why the name RainerScript?)
<https://rainer.gerhards.net/2008/02/introducing-rainerscript-and-some.html>`_
RainerScript supports legacy syslog.conf format, much as you know it
from other syslogds (like sysklogd or the BSD syslogd) as well as previous versions
of rsyslog. Initial work on RainerScript began in v4, and the if-construct was already
supported in v4 and v5. Version 6 has now taken this further. After long discussions we
decided to use the legacy format as a basis, and lightly extend it by native RainerScript
constructs. The main goal was to make sure that previous knowledge and config systems
could still be used while offering a much more intuitive and powerful way of configuring
rsyslog.

RainerScript has been implemented from scratch and with new tools (flex/bison, for those in the
know). Starting with 6.3.3, this new config file processor replaces the legacy one. Note that
the new processor handles all formats, extended RainerScript as well as legacy syslog.conf format.
There are some legacy construct that were especially hard to translate. You'll read about them in
other parts of this document (especially outchannels, which require a format change).

In v6, all legacy formats are supported. In the long term, we may remove some of the ugly
rsyslog-specific constructs. Good candidates are all configuration commands starting with
a dollar sign, like "$ActionFileDefaultTemplate"). However, this will not be the case before
rsyslog v7 or (much more likely) v8/9. Right now, you also need to use these commands, because
not all have already been converted to the new RainerScript format.

In 6.3.3, the new parser is used, but almost none of the extended RainerScript capabilities
are available. They will incrementally be introduced with the following releases. Note that for
some features (most importantly if-then-else nested blocks), the v6 core engine is not
capable enough. It is our aim to provide a much better config language to as many rsyslog
users as quickly as possible. As such, we refrain from doing big engine changes in v6. This
in turn means we cannot introduce some features into RainerScript that we really want to see.
These features will come up with rsyslog v7, which will have even better flow control
capabilities inside the core engine. Note that v7 will fully support v6 RainerScript.
Let us also say that the v6 version is not a low-end quick hack: it offers full-fledged
syslog message processing control, capable of doing the best you can find inside the
industry. We just say that v7 will come up with even more advanced capabilities.

Please note that we tried hard to make the RainerScript parser compatible with
all legacy config files. However, we may have failed in one case or another. So if you
experience problems during config processing, chances are there may be a problem
on the rsyslog side. In that case, please let us know.

Please see the
`blog post about rsyslog 6.3.3 config format
<https://rainer.gerhards.net/2011/07/rsyslog-633-config-format-improvements.html>`_
for details of what is currently supported.

compatibility mode
------------------
Compatibility mode (specified via -c option) has been removed. This was a migration aid from
sysklogd and very early versions of rsyslog. As all major distros now have rsyslog as their
default, and thus ship rsyslog-compliant config files, there is no longer a need for
compatibility mode. Removing it provides easier to maintain code. Also, practice has shown
that many users were confused by compatibility mode (and even some package maintainers got
it wrong). So this not only cleans up the code but rather removes a frequent source of
error.

It must be noted, though, that this means rsyslog is no longer a 100% drop-in replacement
for sysklogd. If you convert an extremely old system, you need to checks its config and
probably need to apply some very mild changes to the config file.

abort on config errors
----------------------
Previous versions accepted some malformedness inside the config file without aborting. This
could lead to some uncertainty about which configuration was actually running. In v6 there
are some situations where config file errors can not be ignored. In these cases rsyslog
emits error messages to stderr, and then exists with a non-zero exit code. It is important
to check for those cases as this means log data is potentially lost.
Please note that
the root problem is the same for earlier versions as well. With them, it was just harder
to spot why things went wrong (and if at all).

Default Batch Sizes
-------------------
Due to their positive effect on performance and comparatively low overhead,
default batch sizes have been increased. Starting with 6.3.4, the action queues
have a default batch size of 128 messages.

Default action queue enqueue timeout
------------------------------------
This timeout previously was 2 seconds, and has been reduced to 50ms (starting with 6.5.0). This change
was made as a long timeout will caused delays in the associated main queue, something
that was quite unexpected to users. Now, this can still happen, but the effect is much
less harsh (but still considerable on a busy system). Also, 50ms should be fairly enough
for most output sources, except when they are really broken (like network disconnect). If
they are really broken, even a 2second timeout does not help, so we hopefully get the best
of both worlds with the new timeout. A specific timeout can of course still be configured,
it is just the timeout that changed.

outchannels
-----------
Outchannels are a to-be-removed feature of rsyslog, at least as far as the config
syntax is concerned. Nevertheless, v6 still supports it, but a new syntax is required
for the action. Let's assume your outchannel is named "channel". The previous syntax was

::

  *.* $channel

This was deprecated in v5 and no longer works in v6. Instead, you need to specify

::

  *.* :omfile:$channel

Note that this syntax is available starting with rsyslog v4. It is important to keep on your
mind that future versions of rsyslog will require different syntax and/or drop outchannel support
completely. So if at all possible, avoid using this feature. If you must use it, be prepared for
future changes and watch announcements very carefully.

ompipe default template
-----------------------
Starting with 6.5.0, ompipe does no longer use the omfile default template.
Instead, the default template must be set via the module load statement.
An example is

::

  module(load="builtin:ompipe" template="myDefaultTemplate")

For obvious reasons, the default template must be defined somewhere in
the config file, otherwise errors will happen during the config load
phase.

omusrmsg
--------
The omusrmsg module is used to send messages to users. In legacy-legacy
config format (that is the very old sysklogd style), it was sufficient to use
just the user name to call this action, like in this example:

::

  *.* rgerhards

This format is very ambiguous and causes headache (see
`blog post on omusrmsg <https://rainer.gerhards.net/2011/07/why-omusrmsg-is-evil-and-how-it-is.html>`_
for details). Thus the format has been superseded by this syntax
(which is legacy format ;-)):

::

  *.* :omusrmsg:rgerhards

That syntax is supported since later subversions of version 4.

Rsyslog v6 still supports the legacy-legacy format, but in a very strict
sense. For example, if multiple users or templates are given, no spaces
must be included in the action line. For example, this works up to v5, but no
longer in v6:

::

  *.* rgerhards, bgerhards

To fix it in a way that is compatible with pre-v4, use (note the removed space!):

::

  *.* rgerhards,bgerhards

Of course, it probably is better to understand in native v6 format:

::

  *.* action(type="omusrmsg" users="rgerhards, bgerhards")

As you see, here you may include spaces between user names.

In the long term, legacy-legacy format will most probably totally disappear,
so it is a wise decision to change config files at least to the legacy
format (with ":omusrmsg:" in front of the name).

Escape Sequences in Script-Based Filters
----------------------------------------
In v5, escape sequences were very simplistic. Inside a string, "\x" meant
"x" with x being any character. This has been changed so that the usual set of
escapes is supported, must importantly "\n", "\t", "\xhh" (with hh being hex digits)
and "\ooo" with (o being octal digits). So if one of these sequences was used
previously, results are obviously different. However, that should not create any
real problems, because it is hard to envision why someone should have done that
(why write "\n" when you can also write "n"?).


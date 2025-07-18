Compatibility Notes for rsyslog v7
==================================

This document describes things to keep in mind when moving from v6 to v7. It 
does not list enhancements nor does it talk about compatibility concerns
introduced by earlier versions (for this, see their respective compatibility
documents). Its focus is primarily on what you need to know if you used v6
and want to use v7 without hassle.

Version 7 builds on the new config language introduced in v6 and extends it.
Other than v6, it not just only extends the config language, but provides
considerable changes to core elements as well. The result is much more power and
ease of use as well (this time that is not contradictionary).

BSD-Style blocks
----------------
BSD style blocks are no longer supported (for good reason). See the
`rsyslog BSD blocks info page <http://www.rsyslog.com/g/BSD>`_
for more information and how to upgrade your config.

CEE-Properties
--------------

In rsyslog v6, CEE properties could not be used across disk-based queues. If this was
done, their content was reset. This was a missing feature in v6. In v7, this feature
has been implemented. Consequently, situations where the previous behaviour were
desired need now to be solved differently. We do not think that this will cause any
problems to anyone, especially as in v6 this was announced as a missing feature.

omusrmsg: using just a username or "*" is deprecated
----------------------------------------------------
In legacy config format, the asterisk denotes writing the message to all users.
This is usually used for emergency messages and configured like this:

::

  *.emerg  *

Unfortunately, the use of this single character conflicts with other uses, for
example with the multiplication operator. While rsyslog up to versions v7.4 preserves the meaning of
asterisk as an action, it is deprecated and will probably be removed in future versions.
Consequently, a warning message is emitted. To make this warning go away, the action must
be explicitly given, as follows:

::

  *.emerg  :omusrmsg:*

The same holds true for user names. For example

::

  *.emerg  john

at a minimum should be rewritten as

::

  *.emerg  :omusrmsg:john

Of course, for even more clarity the new RainerScript style of action can
also be used:

::

  *.emerg  action(type="omusrmsg" users="john")

In Rainer's blog, there is more
`background information on why omusrmsg needed to be changed <https://rainer.gerhards.net/2011/07/why-omusrmsg-is-evil-and-how-it-is.html>`_
available.

omruleset and discard (~) action are deprecated
-----------------------------------------------
Both continue to work, but have been replaced by better alternatives.

The discard action (tilde character) has been replaced by the "stop"
RainerScript directive. It is considered more intuitive and offers slightly
better performance.

The omruleset module has been replaced by the "call" RainerScript directive.
Call permits to execute a ruleset like a subroutine, and does so with much
higher performance than omruleset did. Note that omruleset could be run off
an async queue. This was more a side than a desired effect and is not supported
by the call statement. If that effect was needed, it can simply be simulated by
running the called rulesets actions asynchronously (what in any case is the right
way to handle this).

Note that the deprecated modules emit warning messages when being used.
They tell that the construct is deprecated and which statement is to be used
as replacement. This does **not** affect operations: both modules are still
fully operational and will not be removed in the v7 timeframe.

Retries of output plugins that do not do proper replies
-------------------------------------------------------
Some output plugins may not be able to detect if their target is capable of
accepting data again after an error (technically, they always return OK when
TryResume is called). Previously, the rsyslog core engine suspended such an action
after 1000 successive failures. This lead to potentially a large amount of
errors and error messages. Starting with 7.2.1, this has been reduced to 10
successive failures. This still gives the plugin a chance to recover. In extreme
cases, a plugin may now enter suspend mode where it previously did not do so.
In practice, we do NOT expect that.

omfile: file creation time
--------------------------
Originally, omfile created files immediately during startup, no matter if
they were written to or not. In v7, this has changed. Files are only created
when they are needed for the first time.

Also, in pre-v7 files were created *before* privileges were dropped. This meant
that files could be written to locations where the actual desired rsyslog
user was *not* permitted to. In v7, this has been fixed. This is fix also
the prime reason that files are now created on demand (which is later in the
process and after the privilege drop).

Notes for the 7.3/7.4 branch
----------------------------

"last message repeated n times" Processing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This processing has been optimized and moved to the input side. This results
in usually far better performance and also de-couples different sources
from the same
processing. It is now also integrated in to the more generic rate-limiting
processing.

User-Noticeable Changes
.......................
The code works almost as before, with two exceptions:

* The suppression amount can be different, as the new algorithm
  precisely check's a single source, and while that source is being
  read. The previous algorithm worked on a set of mixed messages
  from multiple sources.
* The previous algorithm wrote a "last message repeated n times" message
  at least every 60 seconds. For performance reasons, we do no longer do
  this but write this message only when a new message arrives or rsyslog
  is shut down.

Note that the new algorithms needs support from input modules. If old
modules which do not have the necessary support are used, duplicate 
messages will most probably not be detected. Upgrading the module code is
simple, and all rsyslog-provided plugins support the new method, so this
should not be a real problem (crafting a solution would result in rather
complex code - for a case that most probably would never happen).

Performance Implications
........................
In general, the new method enables far faster output processing. However, it
needs to be noted that the "last message repeated n" processing needs parsed
messages in order to detect duplicated. Consequently, if it is enabled the
parser step cannot be deferred to the main queue processing thread and
thus must be done during input processing. The changes workload distribution
and may have (good or bad) effect on the overall performance. If you have
a very high performance installation, it is suggested to check the performance
profile before deploying the new version.

**Note:** for high-performance
environments it is highly recommended NOT to use "last message repeated n times"
processing but rather the other (more efficient) rate-limiting methods. These
also do NOT require the parsing step to be done during input processing.

Stricter string-template Processing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Previously, no error message for invalid string template parameters
was generated.
Rather a malformed template was generated, and error information emitted
at runtime. However, this could be quite confusing. Note that the new code
changes user experience: formerly, rsyslog and the affected
actions properly started up, but the actions did not produce proper
data. Now, there are startup error messages and the actions are NOT
executed (due to missing template due to template error).

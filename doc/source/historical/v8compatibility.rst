Compatibility Notes for rsyslog v8
==================================

This document describes things to keep in mind when moving from v7 to
v8. It does not list enhancements nor does it talk about compatibility
concerns introduced by earlier versions (for this, see their respective
compatibility documents). Its focus is primarily on what you need to
know if you used v7 and want to use v8 without hassle.

Version 8 offers a completely rewritten core rsyslog engine. This
resulted in a number of changes that are visible to users and (plugin)
developers. Most importantly, pre-v8 plugins **do not longer work** and
need to be updated to support the new calling interfaces. If you
developed a plugin, be sure to review the developer section below.

Mark Messages
-------------

In previous versions, mark messages were by default only processed if an
action was not executed for some time. The default has now changed, and
mark messages are now always processed. Note that this enables faster
processing inside rsyslog. To change to previous behaviour, you need to
add action.writeAllMarkMessages="off" to the actions in question.

Untested Modules
----------------

The following modules have been updated and successfully build, but no
"real" test were conducted. Users of these modules should use extra
care.

-  mmsequence
-  plugins/omgssapi
-  omsnmp
-  mmfields
-  mmpstrucdata
-  plugins/mmaudit
-  ommongodb - larger changes still outstanding
-  ompgsql
-  plugins/omrabbitmq - not project supported
-  plugins/omzmq3 - not project supported
-  plugins/omhdfs (transaction support should be improved, requires sponsor)
-  omuxsock

In addition to bug reports, success reports are also appreciated for
these modules (this may save us testing).

What Developers need to Know
----------------------------

output plugin interface
~~~~~~~~~~~~~~~~~~~~~~~

To support the new core engine, the output interface has been
considerably changed. It is suggested to review some of the
project-provided plugins for full details. In this doc, we describe the
most important changes from a high level perspective.

**Multi-thread awareness required**

The new engine activates one **worker**\ instance of output actions on
each worker thread. This means an action has now three types of data:

-  global
-  action-instance - previously known pData, one for each action inside
   the config
-  worker-action-instance - one for each worker thread (called
   pWrkrData), note that this is specific to exactly one pData

The plugin **must** now by multi-threading aware. It may be called by
multiple threads concurrently, but it is guaranteed that each call is
for a unique pWrkrData structure. This still permits to write plugins
easily, but enables the engine to work with much higher performance.
Note that plugin developers should assume it is the norm that multiple
concurrent worker action instances are active a the some time.

**New required entry points**

In order to support the new threading model, new entry points are
required. Most importantly, only the plugin knows which data must be
present in pData and pWrkrData, so it must created and destroy these
data structures on request of the engine. Note that pWrkrData may be
destroyed at any time and new ones re-created later. Depending on
workload structure and configuration, this can happen frequently.

New entry points are:

-  createWrkrInstance
-  freeWrkrInstance

The calling interface for these entry points has changed. Basically,
they now receive a pWrkrData object instead pData. It is assumed that
createWrkrInstance populates pWrkrData->pData appropriately.

-  beginTransaction
-  doAction
-  endTransaction

**Changed entry points**

Some of the existing entry points have been changed.

The **doAction** entry point formerly got a variable ``iMsgOpts`` which
is no longer provided. This variable was introduced in early days and
exposed some internal message state information to the output module.
Review of all known existing plugins showed that none except omfile ever
used that variable. And omfile only did so to do some no longer required
legacy handling.

In essence, it is highly unlikely that you ever accessed this variable.
So we expect that nobody actually notices that the variable has gone
away.

Removal of the variable provides a slight performance gain, as we do no
longer need to maintain it inside the output system (which leads to less
CPU instructions and better cache hits).

**RS\_RET\_SUSPENDED is no longer supported when creating an action
instance**

This means a plugin must not try to establish any connections or the
like before any of its processing entry points (like beginTransaction or
doAction) is called. This was generally also the case with v7, but was
not enforced in all cases. In v8, creating action fails if
anything but RS\_RET\_OK is returned.

string generator interface
~~~~~~~~~~~~~~~~~~~~~~~~~~

Bottom line: string generators need to be changed or will abort.

The BEGINstrgen() entry point has greatly changed. Instead of two
parameters for the output buffers, they now receive a single ``iparam``
pointer, which contains all data items needed. Also, the message pointer
is now const to "prevent" (accidental) changes to the message via the
strgen interface.

Note that strgen modules must now maintain the iparam->lenStr field,
which must hold the length of the generated string on exit. This is
necessary as we cache the string sizes in order to reduced strlen()
calls. Also, the numerical parameters are now unsigned and no longer
size\_t. This permits us to store them directly into optimized heap
structures.

Specifics for Version 8.3 and 8.4
---------------------------------

Unsupported Command Line Options Removed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The command line options a, h, m, o, p, g, r, t and c were not 
supported since many versions. However, they spit out an error
message that they were unsupported. This error message now no
longer appears, instead the regular usage() display happens.
This should not have any effect to users.


Specifics for Version 8.5 and 8.6
---------------------------------

imfile changes
~~~~~~~~~~~~~~

Starting with 8.5.0, imfile supports wildcards in file names, but
does do so only in inotify mode. In order to support wildcards, the
handling of statefile needed to be changed. Most importantly, the
*statefile* input parameter has been deprecated. See
:doc:`imfile module documentation <../../configuration/modules/imfile>`
for more details.

Command Line Options
~~~~~~~~~~~~~~~~~~~~
There is a small set of configuration command line options available dating back
to the dark ages of syslog technology. Setting command-line options is
distro specific and a hassle for most users. As such, we are phasing out
these options, and will do so rather quickly.

Some of them (most notably -l, -s) will completely be removed, as
feedback so far indicated they are no longer in use. Others will be
replaced by proper configuration objects.

**Expect future rsyslog versions to no longer accept those configuration
command line options.**

Please see this table to see what to use as a replacement for the
current options:

==========  ===========================================================================
**Option**  **replacement**
-4          global(net.ipprotocol="ipv4-only")
-6          global(net.ipprotocol="ipv6-only")
-A          omfwd input parameter "udp.sendToAll"
-l          dropped, currently no replacement
-q          global(net.aclAddHostnameOnFail="on")
-Q          global(net.aclResolveHostname="off")
-s          dropped, currently no replacement
-S          omrelp action parameter "localclientip"
-w          global(net.permitACLWarning="off")
-x          global(net.enableDNS="off")
==========  ===========================================================================


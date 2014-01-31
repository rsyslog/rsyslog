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

Modules WITHOUT v8 Support
--------------------------

The following modules will not properly build under rsyslog v8 and need
to be updated:

-  plugins/omgssapi
-  plugins/omhdfs
-  plugins/omrabbitmq - not project supported
-  plugins/omzmq3 - not project supported
-  plugins/mmrfc5424addhmac - was a custom project, requires sponsoring
   for conversion
-  plugins/omoracle - orphaned since a while -- use omlibdbi instead
-  plugins/mmsnmptrapd - waiting for demand
-  plugins/mmcount - scheduled to be updated soon

Untested Modules
----------------

The following modules have been updated and successfully build, but no
"real" test were conducted. Users of these modules should use extra
care.

-  mmsequence
-  omprog
-  omsnmp
-  mmfields
-  mmpstrucdata
-  plugins/mmaudit
-  omlibdbi - will be tested soon
-  ommongodb - larger changes still outstanding
-  ompgsql - larger chages still outstanding
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

**NOTE: the v8 output module interface is not yet stable.** It is highly
likely that additional changes will be made within the next weeks. So if
you convert a module now, be prepared for additional mandatory changes.

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
doAction) is called. This was generally also the case von v7, but was
not enforced in all cases. In v8, creating action creation fails if
anything but RS\_RET\_OK is returned.

string generator interface
~~~~~~~~~~~~~~~~~~~~~~~~~~

Bottom line: string generators need to be changed or will abort.

The BEGINstrgen() entry point has greatly changed. Instead of two
parameters for the output buffers, they now receive a single ``iparam``
pointer, which contains all data items needed. Also, the message pointer
is now const to "prevent" (accidential) changes to the message via the
strgen interface.

Note that strgen modules must now maintain the iparam->lenStr field,
which must hold the length of the generated string on exit. This is
necessary as we cache the string sizes in order to reduced strlen()
calls. Also, the numerical parameters are now unsigned and no longer
size\_t. This permits us to store them directly into optimized heap
structures.

[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2013 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.

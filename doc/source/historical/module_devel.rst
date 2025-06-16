Developing rsyslog modules (outdated)
=====================================

*Written by `Rainer Gerhards* <https://rainer.gerhards.net/>`_ *(2007-07-28)*

**This document is outdated and primarily contains historical
information. Do not trust it to build code. It currently is under
review.**

Overview
--------

In theory, modules provide input and output, among other functions, in
rsyslog. In practice, modules are only utilized for output in the
current release. The module interface is not yet completed and a moving
target. We do not recommend to write a module based on the current
specification. If you do, please be prepared that future released of
rsyslog will probably break your module.

A goal of modularization is to provide an easy to use plug-in interface.
However, this goal is not yet reached and all modules must be statically
linked.

Module "generation"
-------------------

There is a lot of plumbing that is always the same in all modules. For
example, the interface definitions, answering function pointer queries
and such. To get rid of these laborious things, I generate most of them
automatically from a single file. This file is named module-template.h.
It also contains the current best description of the interface
"specification".

One thing that can also be achieved with it is the capability to cope
with a rapidly changing interface specification. The module interface is
evolving. Currently, it is far from being finished. As I moved the
monolithic code to modules, I needed (and still need) to make many
"non-clean" code hacks, just to get it working. These things are now
gradually being removed. However, this requires frequent changes to the
interfaces, as things move in and out while working towards a clean
interface. All the interim is necessary to reach the goal. This
volatility of specifications is the number one reasons I currently
advise against implementing your own modules (hint: if you do, be sure
to use module-template.h and be prepared to fix newly appearing and
disappearing data elements).

Naming Conventions
------------------

Source
~~~~~~

Output modules, and only output modules, should start with a file name
of "om" (e.g. "omfile.c", "omshell.c"). Similarly, input modules will
use "im" and filter modules "fm". The third character shall not be a
hyphen.

Module Security
---------------

Modules are directly loaded into rsyslog's address space. As such, any
module is provided a big level of trust. Please note that further module
interfaces might provide a way to load a module into an isolated address
space. This, however, is far from being completed. So the number one
rule about module security is to run only code that you know you can
trust.

To minimize the security risks associated with modules, rsyslog provides
only the most minimalistic access to data structures to its modules. For
that reason, the output modules do not receive any direct pointers to
the selector\_t structure, the syslogd action structures and - most
importantly - the msg structure itself. Access to these structures would
enable modules to access data that is none of their business, creating a
potential security weakness.

Not having access to these structures also simplifies further queueing
and error handling cases. As we do not need to provide e.g. full access
to the msg object itself, we do not need to serialize and cache it.
Instead, strings needed by the module are created by syslogd and then
the final result is provided to the module. That, for example, means
that in a queued case $NOW is the actual timestamp of when the message
was processed, which may be even days before it being dequeued. Think
about it: If we wouldn't cache the resulting string, $NOW would be the
actual date if the action were suspended and messages queued for some
time. That could potentially result in big confusion.

It is thought that if an output module actually needs access to the
while msg object, we will (then) introduce a way to serialize it (e.g.
to XML) in the property replacer. Then, the output module can work with
this serialized object. The key point is that output modules never deal
directly with msg objects (and other internal structures). Besides
security, this also greatly simplifies the job of the output module
developer.

Action Selectors
----------------

Modules (and rsyslog) need to know when they are called. For this, there
must an action identification in selector lines. There are two
syntaxes: the single-character syntax, where a single characters
identifies a module (e.g. "\*" for a wall message) and the modules
designator syntax, where the module name is given between colons (e.g.
":ommysql:"). The single character syntax is depreciated and should not
be used for new plugins.

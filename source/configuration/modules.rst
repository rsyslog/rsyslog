Modules
=======

**This document is outdated and primarily contains historical
information. Do not trust it to build code. It currently is under
review.**

This document is incomplete. The module interface is also quite
incomplete and under development. Do not currently use it! You may
want to visit `Rainer's blog <http://rgerhards.blogspot.com/>`_ to learn
what's going on.

Rsyslog has a modular design. This enables functionality to be
dynamically loaded from modules, which may also be written by any third
party. Rsyslog itself offers all non-core functionality as modules.
Consequently, there is a growing number of modules. Here is the entry
point to their documentation and what they do (list is currently not
complete)

Please note that each module provides configuration directives, which
are NOT necessarily being listed below. Also remember, that a modules
configuration directive (and functionality) is only available if it has
been loaded (using $ModLoad).

It is relatively easy to write a rsyslog module. **If none of the
provided modules solve your need, you may consider writing one or have
one written for you by `Adiscon's professional services for
rsyslog <http://www.rsyslog.com/professional-services>`_**\ (this often
is a very cost-effective and efficient way of getting what you need).

There exist different classes of loadable modules:

-  `Input Modules <rsyslog_conf_modules.html#im>`_
-  `Output Modules <rsyslog_conf_modules.html#om>`_
-  `Parser Modules <rsyslog_conf_modules.html#pm>`_
-  `Message Modification Modules <rsyslog_conf_modules.html#mm>`_
-  `String Generator Modules <rsyslog_conf_modules.html#sm>`_
-  `Library Modules <rsyslog_conf_modules.html#lm>`_

Input Modules
-------------

Input modules are used to gather messages from various sources. They
interface to message generators.

-  `imfile <modules/imfile.html>`_ -  input module for text files
-  `imrelp <modules/imrelp.html>`_ - RELP input module
-  `imudp <modules/imudp.html>`_ - udp syslog message input
-  `imtcp <modules/imtcp.html>`_ - input plugin for tcp syslog
-  `imptcp <modules/imptcp.html>`_ - input plugin for plain tcp syslog (no TLS
   but faster)
-  `imgssapi <modules/imgssapi.html>`_ - input plugin for plain tcp and
   GSS-enabled syslog
-  immark - support for mark messages
-  `imklog <modules/imklog.html>`_ - kernel logging
-  `imuxsock <modules/imuxsock.html>`_ - unix sockets, including the system log
   socket
-  `imsolaris <modules/imsolaris.html>`_ - input for the Sun Solaris system log
   source
-  `im3195 <modules/im3195.html>`_ - accepts syslog messages via RFC 3195
-  `impstats <modules/impstats.html>`_ - provides periodic statistics of rsyslog
   internal counters
-  `imjournal <modules/imjournal.html>`_ - Linux journal inuput module

Output Modules
--------------

Output modules process messages. With them, message formats can be
transformed and messages be transmitted to various different targets.

-  `omfile <modules/omfile.html>`_ - file output module
-  `omfwd <modules/omfwd.html>`_ (does NOT yet work in v8) - syslog forwarding
   output module
-  `omjournal <modules/omjournal.html>`_ - Linux journal output module
-  `ompipe <modules/ompipe.html>`_ - named pipe output module
-  `omusrmsg <modules/omusrmsg.html>`_ - user message output module
-  `omsnmp <modules/omsnmp.html>`_ (does NOT yet work in v8) - SNMP trap output
   module
-  `omtdout <modules/omstdout.html>`_ - stdout output module (mainly a test
   tool)
-  `omrelp <modules/omrelp.html>`_ (does NOT yet work in v8) - RELP output
   module
-  `omruleset <modules/omruleset.html>`_ - forward message to another ruleset
-  omgssapi (does NOT yet work in v8) - output module for GSS-enabled
   syslog
-  `ommysql <modules/ommysql.html>`_ - output module for MySQL
-  ompgsql (does NOT yet work in v8) - output module for PostgreSQL
-  `omlibdbi <modules/omlibdbi.html>`_ (does NOT yet work in v8) - generic
   database output module (Firebird/Interbase, MS SQL, Sybase, SQLLite,
   Ingres, Oracle, mSQL)
-  `ommail <modules/ommail.html>`_ (does NOT yet work in v8) - permits rsyslog
   to alert folks by mail if something important happens
-  `omprog <modules/omprog.html>`_ (does NOT yet work in v8) - permits sending
   messages to a program for custom processing
-  `omoracle <modules/omoracle.html>`_ (orphaned) - output module for Oracle
   (native OCI interface)
-  `omudpspoof <modules/omudpspoof.html>`_ (does NOT yet work in v8) - output
   module sending UDP syslog messages with a spoofed address
-  `omuxsock <modules/omuxsock.html>`_ (does NOT yet work in v8) - output module
   Unix domain sockets
-  `omhdfs <modules/omhdfs.html>`_ (does NOT yet work in v8) - output module for
   Hadoop's HDFS file system
-  `ommongodb <modules/ommongodb.html>`_ (does NOT yet work in v8) - output
   module for MongoDB
-  `omelasticsearch <modules/omelasticsearch.html>`_ - output module for
   ElasticSearch

Parser Modules
--------------

Parser modules are used to parse message content, once the message has
been received. They can be used to process custom message formats or
invalidly formatted messages. For details, please see the `rsyslog
message parser documentation <messageparser.html>`_.

The current modules are currently provided as part of rsyslog:

-  `pmrfc5424[builtin] <modules/pmrfc5424.html>`_ - rsyslog.rfc5424 - parses RFC5424-formatted
   messages (the new syslog standard)
-  `pmrfc3164[builtin] <modules/pmrfc3164.html>`_ - rsyslog.rfc3164 - the traditional/legacy syslog
   parser
-  `pmrfc3164sd <modules/pmrfc3164sd.html>`_ - rsyslog.rfc3164sd - a contributed module supporting
   RFC5424 structured data inside RFC3164 messages (not supported by the
   rsyslog team)
-  `pmlastmsg <modules/pmlastmsg.html>`_ - rsyslog.lastmsg - a parser module
   that handles the typically malformed "last messages repated n times"
   messages emitted by some syslogds.

Message Modification Modules
----------------------------

Message modification modules are used to change the content of messages
being processed. They can be implemented using either the output module
or the parser module interface. From the rsyslog core's point of view,
they actually are output or parser modules, it is their implementation
that makes them special.

Currently, there exists only a limited set of such modules, but new ones
could be written with the methods the engine provides. They could be
used, for example, to add dynamically computed content to message
(fields).

Message modification modules are usually written for one specific task
and thus usually are not generic enough to be reused. However, existing
module's code is probably an excellent starting base for writing a new
module. Currently, the following modules exist inside the source tree:

-  `mmanon <modules/mmanon.html>`_ - used to anonymize log messages.
-  `mmcount <../mmcount.html>`_ - message modification plugin which counts
   messages
-  `mmfields <../mmfields.html>`_ - used to extract fields from specially
   formatted messages (e.g. CEF)
-  `mmnormalize <modules/mmnormalize.html>`_ - used to normalize log messages.
   Note that this actually is a **generic** module.
-  `mmjsonparse <modules/mmjsonparse.html>`_ - used to interpret CEE/lumberjack
   enabled structured log messages.
-  `mmpstrucdata <../mmpstrucdata.html>`_ - used to parse RFC5424
   structured data into json message properties
-  `mmsnmptrapd <modules/mmsnmptrapd.html>`_ - uses information provided by
   snmptrapd inside the tag to correct the original sender system and
   priority of messages. Implemented via the output module interface.
-  `mmutf8fix <../mmutf8fix.html>`_ - used to fix invalid UTF-8 character
   sequences
-  `mmrfc5424addhmac <../mmrfc5424addhmac.html>`_ - custom module for
   adding HMACs to rfc5424-formatted messages if not already present
-  `mmsequence <../mmsequence.html>`_ - sequence generator and counter
   plugin

String Generator Modules
------------------------

String generator modules are used, as the name implies, to generate
strings based on the message content. They are currently tightly coupled
with the template system. Their primary use is to speed up template
processing by providing a native C interface to template generation.
These modules exist since 5.5.6. To get an idea of the potential
speedup, the default file format, when generated by a string generator,
provides a roughly 5% speedup. For more complex strings, especially
those that include multiple regular expressions, the speedup may be
considerably higher.

String generator modules are written to a quite simple interface.
However, a word of caution is due: they access the rsyslog message
object via a low-level interface. That interface is not guaranteed yet
to stay stable. So it may be necessary to modify string generator
modules if the interface changes. Obviously, we will not do that without
good reason, but it may happen.

Rsyslog comes with a set of core, build-in string generators, which are
used to provide those default templates that we consider to be
time-critical:

-  smfile - the default rsyslog file format
-  smfwd - the default rsyslog (network) forwarding format
-  smtradfile - the traditional syslog file format
-  smfwd - the traditional syslog (network) forwarding format

Note that when you replace these defaults be some custom strings, you
will loose some performance (around 5%). For typical systems, this is
not really relevant. But for a high-performance systems, it may be very
relevant. To solve that issue, create a new string generator module for
your custom format, starting out from one of the default generators
provided. If you can not do this yourself, you may want to contact
`Adiscon <mailto:info%40adiscon.com>`_ as we offer custom development of
string generators at a very low price.

Note that string generator modules can be dynamically loaded. However,
the default ones provided are so important that they are build right
into the executable. But this does not need to be done that way (and it
is straightforward to do it dynamic).


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

Library Modules
---------------

Library modules provide dynamically loadable functionality for parts of
rsyslog, most often for other loadable modules. They can not be
user-configured and are loaded automatically by some components. They
are just mentioned so that error messages that point to library moduls
can be understood. No module list is provided.

Where are the modules integrated into the Message Flow?
-------------------------------------------------------

Depending on their module type, modules may access and/or modify
messages at various stages during rsyslog's processing. Note that only
the "core type" (e.g. input, output) but not any type derived from it
(message modification module) specifies when a module is called.

The simplified workflow is as follows:

.. figure:: module_workflow.png
   :align: center
   :alt: 

As can be seen, messages are received by input modules, then passed to
one or many parser modules, which generate the in-memory representation
of the message and may also modify the message itself. The, the internal
representation is passed to output modules, which may output a message
and (with the interfaces newly introduced in v5) may also modify
messageo object content.

String generator modules are not included inside this picture, because
they are not a required part of the workflow. If used, they operate "in
front of" the output modules, because they are called during template
generation.

Note that the actual flow is much more complex and depends a lot on
queue and filter settings. This graphic above is a high-level message
flow diagram.


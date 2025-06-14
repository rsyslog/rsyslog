**********************************
imuxsock: Unix Socket Input Module
**********************************

===========================  ===========================================================================
**Module Name:**             **imuxsock**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides the ability to accept syslog messages from applications
running on the local system via Unix sockets. Most importantly, this is the
mechanism by which the :manpage:`syslog(3)` call delivers syslog messages
to rsyslogd.

.. seealso::

   :doc:`omuxsock`


Notable Features
================

- :ref:`imuxsock-rate-limiting-label`
- :ref:`imuxsock-trusted-properties-label`
- :ref:`imuxsock-flow-control-label`
- :ref:`imuxsock-application-timestamps-label`
- :ref:`imuxsock-systemd-details-label`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Module Parameters
-----------------

.. warning::

   When running under systemd, **many "sysSock." parameters are ignored**.
   See parameter descriptions and the :ref:`imuxsock-systemd-details-label` section for
   details.


SysSock.IgnoreTimestamp
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$SystemLogSocketIgnoreMsgTimestamp``"

Ignore timestamps included in the messages, applies to messages received via
the system log socket.


SysSock.IgnoreOwnMessages
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Ignores messages that originated from the same instance of rsyslogd.
There usually is no reason to receive messages from ourselves. This
setting is vital when writing messages to the systemd journal.

.. versionadded:: 7.3.7

.. seealso::

   See :doc:`omjournal <omjournal>` module documentation for a more
   in-depth description.


SysSock.Use
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$OmitLocalLogging``"

Listen on the default local log socket (``/dev/log``) or, if provided, use
the log socket value assigned to the ``SysSock.Name`` parameter instead
of the default. This is most useful if you run multiple instances of
rsyslogd where only one shall handle the system log socket.  Unless
disabled by the ``SysSock.Unlink`` setting, this socket is created
upon rsyslog startup and deleted upon shutdown, according to
traditional syslogd behavior.

The behavior of this parameter is different for systemd systems. For those
systems, ``SysSock.Use`` still needs to be enabled, but the value of
``SysSock.Name`` is ignored and the socket provided by systemd is used
instead. If this parameter is *not* enabled, then imuxsock will only be
of use if a custom input is configured.

See the :ref:`imuxsock-systemd-details-label` section for details.


SysSock.Name
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "/dev/log", "no", "``$SystemLogSocketName``"

Specifies an alternate log socket to be used instead of the default system
log socket, traditionally ``/dev/log``. Unless disabled by the
``SysSock.Unlink`` setting, this socket is created upon rsyslog startup
and deleted upon shutdown, according to traditional syslogd behavior.

The behavior of this parameter is different for systemd systems. See the
 :ref:`imuxsock-systemd-details-label` section for details.


SysSock.FlowControl
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$SystemLogFlowControl``"

Specifies if flow control should be applied to the system log socket.


SysSock.UsePIDFromSystem
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$SystemLogUsePIDFromSystem``"

Specifies if the pid being logged shall be obtained from the log socket
itself. If so, the TAG part of the message is rewritten. It is recommended
to turn this option on, but the default is "off" to keep compatible
with earlier versions of rsyslog.

.. versionadded:: 5.7.0


SysSock.RateLimit.Interval
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "", "no", "``$SystemLogRateLimitInterval``"

Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting. Set it to a number of seconds (5
recommended) to activate rate-limiting. The default of 0 has been
chosen as people experienced problems with this feature activated
by default. Now it needs an explicit opt-in by setting this parameter.


SysSock.RateLimit.Burst
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200", "(2^31)-1", "no", "``$SystemLogRateLimitBurst``"

Specifies the rate-limiting burst in number of messages.

.. versionadded:: 5.7.1


SysSock.RateLimit.Severity
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "", "no", "``$SystemLogRateLimitSeverity``"

Specifies the severity of messages that shall be rate-limited.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog#Severity_level


SysSock.UseSysTimeStamp
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$SystemLogUseSysTimeStamp``"

The same as the input parameter ``UseSysTimeStamp``, but for the system log
socket. This parameter instructs ``imuxsock`` to obtain message time from
the system (via control messages) instead of using time recorded inside
the message. This may be most useful in combination with systemd. Due to
the usefulness of this functionality, we decided to enable it by default.
As such, the behavior is slightly different than previous versions.
However, we do not see how this could negatively affect existing environments.

.. versionadded:: 5.9.1


SysSock.Annotate
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$SystemLogSocketAnnotate``"

Turn on annotation/trusted properties for the system log socket. See
the :ref:`imuxsock-trusted-properties-label` section for more info.


SysSock.ParseTrusted
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$SystemLogParseTrusted``"

If ``SysSock.Annotation`` is turned on, create JSON/lumberjack properties
out of the trusted properties (which can be accessed via |FmtAdvancedName|
JSON Variables, e.g. ``$!pid``) instead of adding them to the message.

.. versionadded:: 7.2.7
   |FmtAdvancedName| directive introduced

.. versionadded:: 7.3.8
   |FmtAdvancedName| directive introduced

.. versionadded:: 6.5.0
   |FmtObsoleteName| directive introduced


SysSock.Unlink
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

If turned on (default), the system socket is unlinked and re-created
when opened and also unlinked when finally closed. Note that this
setting has no effect when running under systemd control (because
systemd handles the socket. See the :ref:`imuxsock-systemd-details-label`
section for details.

.. versionadded:: 7.3.9


SysSock.UseSpecialParser
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

The equivalent of the ``UseSpecialParser`` input parameter, but
for the system socket. If turned on (the default) a special parser is
used that parses the format that is usually used
on the system log socket (the one :manpage:`syslog(3)` creates). If set to
"off", the regular parser chain is used, in which case the format on the
log socket can be arbitrary.

.. note::

   When the special parser is used, rsyslog is able to inject a more precise
   timestamp into the message (it is obtained from the log socket). If the
   regular parser chain is used, this is not possible.

.. versionadded:: 8.9.0
   The setting was previously hard-coded "on"


SysSock.ParseHostname
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. note::

   This option only has an effect if ``SysSock.UseSpecialParser`` is
   set to "off".

Normally, the local log sockets do *not* contain hostnames. If set
to on, parsers will expect hostnames just like in regular formats. If
set to off (the default), the parser chain is instructed to not expect
them.

.. versionadded:: 8.9.0


Input Parameters
----------------

Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "default ruleset", "no", "none"

Binds specified ruleset to this input. If not set, the default
ruleset is bound.

.. versionadded:: 8.17.0


IgnoreTimestamp
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$InputUnixListenSocketIgnoreMsgTimestamp``"

Ignore timestamps included in messages received from the input being
defined.


IgnoreOwnMessages
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Ignore messages that originated from the same instance of rsyslogd.
There usually is no reason to receive messages from ourselves. This
setting is vital when writing messages to the systemd journal.

.. versionadded:: 7.3.7

.. seealso::

   See :doc:`omjournal <omjournal>` module documentation for a more
   in-depth description.




FlowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputUnixListenSocketFlowControl``"

Specifies if flow control should be applied to the input being defined.


RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "", "no", "``$IMUXSockRateLimitInterval``"

Specifies the rate-limiting interval in seconds. Default value is 0, which
turns off rate limiting. Set it to a number of seconds (5 recommended)
to activate rate-limiting. The default of 0 has been chosen as people
experienced problems with this feature activated by default. Now it
needs an explicit opt-in by setting this parameter.


RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200", "(2^31)-1", "no", "``$IMUXSockRateLimitBurst``"

Specifies the rate-limiting burst in number of messages.

.. versionadded:: 5.7.1


RateLimit.Severity
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "", "no", "``$IMUXSockRateLimitSeverity``"

Specifies the severity of messages that shall be rate-limited.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog#Severity_level


UsePIDFromSystem
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputUnixListenSocketUsePIDFromSystem``"

Specifies if the pid being logged shall be obtained from the log socket
itself. If so, the TAG part of the message is rewritten. It is
recommended to turn this option on, but the default is "off" to keep
compatible with earlier versions of rsyslog.


UseSysTimeStamp
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$InputUnixListenSocketUseSysTimeStamp``"

This parameter instructs ``imuxsock`` to obtain message time from
the system (via control messages) instead of using time recorded inside
the message. This may be most useful in combination with systemd. Due to
the usefulness of this functionality, we decided to enable it by default.
As such, the behavior is slightly different than previous versions.
However, we do not see how this could negatively affect existing environments.

.. versionadded:: 5.9.1


CreatePath
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputUnixListenSocketCreatePath``"

Create directories in the socket path if they do not already exist.
They are created with 0755 permissions with the owner being the
process under which rsyslogd runs. The default is not to create
directories. Keep in mind, though, that rsyslogd always creates
the socket itself if it does not exist (just not the directories
by default).

This option is primarily considered useful for defining additional
sockets that reside on non-permanent file systems. As rsyslogd probably
starts up before the daemons that create these sockets, it is a vehicle
to enable rsyslogd to listen to those sockets even though their directories
do not yet exist.

.. versionadded:: 4.7.0
.. versionadded:: 5.3.0


Socket
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$AddUnixListenSocket``"

Adds additional unix socket. Formerly specified with the ``-a`` option.


HostName
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "NULL", "no", "``$InputUnixListenSocketHostName``"

Allows overriding the hostname that shall be used inside messages
taken from the input that is being defined.


Annotate
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputUnixListenSocketAnnotate``"

Turn on annotation/trusted properties for the input that is being defined.
See the :ref:`imuxsock-trusted-properties-label` section for more info.


ParseTrusted
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ParseTrusted``"

Equivalent to the ``SysSock.ParseTrusted`` module parameter, but applies
to the input that is being defined.


Unlink
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``none``"

If turned on (default), the socket is unlinked and re-created when opened
and also unlinked when finally closed. Set it to off if you handle socket
creation yourself.

.. note::

   Note that handling socket creation oneself has the
   advantage that a limited amount of messages may be queued by the OS
   if rsyslog is not running.

.. versionadded:: 7.3.9


UseSpecialParser
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Equivalent to the ``SysSock.UseSpecialParser`` module parameter, but applies
to the input that is being defined.

.. versionadded:: 8.9.0
   The setting was previously hard-coded "on"


ParseHostname
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Equivalent to the ``SysSock.ParseHostname`` module parameter, but applies
to the input that is being defined.

.. versionadded:: 8.9.0


.. _imuxsock-rate-limiting-label:

Input rate limiting
===================

rsyslog supports (optional) input rate limiting to guard against the problems
of a wild running logging process. If more than
``SysSock.RateLimit.Interval`` \* ``SysSock.RateLimit.Burst`` log messages
are emitted from the same process, those messages with
``SysSock.RateLimit.Severity`` or lower will be dropped. It is not possible
to recover anything about these messages, but imuxsock will tell you how
many it has dropped once the interval has expired AND the next message is
logged. Rate-limiting depends on ``SCM\_CREDENTIALS``. If the platform does
not support this socket option, rate limiting is turned off. If multiple
sockets are configured, rate limiting works independently on each of
them (that should be what you usually expect).

The same functionality is available for additional log sockets, in which
case the config statements just use the prefix RateLimit... but otherwise
works exactly the same. When working with severities, please keep in mind
that higher severity numbers mean lower severity and configure things
accordingly. To turn off rate limiting, set the interval to zero.

.. versionadded:: 5.7.1


.. _imuxsock-trusted-properties-label:

Trusted (syslog) properties
===========================

rsyslog can annotate messages from system log sockets (via imuxsock) with
so-called `Trusted syslog
properties <http://www.rsyslog.com/what-are-trusted-properties/>`_, (or just
"Trusted Properties" for short). These are message properties not provided by
the logging client application itself, but rather obtained from the system.
As such, they can not be faked by the user application and are trusted in
this sense. This feature is based on a similar idea introduced in systemd.

This feature requires a recent enough Linux Kernel and access to
the ``/proc`` file system. In other words, this may not work on all
platforms and may not work fully when privileges are dropped (depending
on how they are dropped). Note that trusted properties can be very
useful, but also typically cause the message to grow rather large. Also,
the format of log messages is changed by adding the trusted properties at
the end. For these reasons, the feature is **not enabled by default**.
If you want to use it, you must turn it on (via
``SysSock.Annotate`` and ``Annotate``).

.. versionadded:: 5.9.4

.. seealso::

   `What are "trusted properties"?
   <http://www.rsyslog.com/what-are-trusted-properties/>`_


.. _imuxsock-flow-control-label:

Flow-control of Unix log sockets
================================

If processing queues fill up, the unix socket reader is blocked for a
short while to help prevent overrunning the queues. If the queues are
overrun, this may cause excessive disk-io and impact performance.

While turning on flow control for many systems does not hurt, it `can` lead
to a very unresponsive system and as such is disabled by default.

This means that log records are placed as quickly as possible into the
processing queues. If you would like to have flow control, you
need to enable it via the ``SysSock.FlowControl`` and ``FlowControl`` config
directives. Just make sure you have thought about the implications and have
tested the change on a non-production system first.


.. _imuxsock-application-timestamps-label:

Control over application timestamps
===================================

Application timestamps are ignored by default. This is needed, as some
programs (e.g. sshd) log with inconsistent timezone information, what
messes up the local logs (which by default don't even contain time zone
information). This seems to be consistent with what sysklogd has done for
many years. Alternate behaviour may be desirable if gateway-like processes
send messages via the local log slot. In that case, it can be enabled via
the ``SysSock.IgnoreTimestamp`` and ``IgnoreTimestamp`` config directives.


.. _imuxsock-systemd-details-label:

Coexistence with systemd
========================

Rsyslog should by default be configured for systemd support on all platforms
that usually run systemd (which means most Linux distributions, but not, for
example, Solaris).

Rsyslog is able to coexist with systemd with minimal changes on the part of the
local system administrator. While the ``systemd journal`` now assumes full
control of the local ``/dev/log`` system log socket, systemd provides
access to logging data via the ``/run/systemd/journal/syslog`` log socket.
This log socket is provided by the ``syslog.socket`` file that is shipped
with systemd.

The imuxsock module can still be used in this setup and provides superior
performance over :doc:`imjournal <imjournal>`, the alternative journal input
module.

.. note::

   It must be noted, however, that the journal tends to drop messages
   when it becomes busy instead of forwarding them to the system log socket.
   This is because the journal uses an async log socket interface for forwarding
   instead of the traditional synchronous one.

.. versionadded:: 8.32.0
   rsyslog emits an informational message noting the system log socket provided
   by systemd.

.. seealso::

   :doc:`imjournal`


Handling of sockets
-------------------

What follows is a brief description of the process rsyslog takes to determine
what system socket to use, which sockets rsyslog should listen on, whether
the sockets should be created and how rsyslog should handle the sockets when
shutting down.

.. seealso::

   `Writing syslog Daemons Which Cooperate Nicely With systemd
   <https://www.freedesktop.org/wiki/Software/systemd/syslog/>`_


Step 1: Select name of system socket
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#. If the user has not explicitly chosen to set ``SysSock.Use="off"`` then
   the default listener socket (aka, "system log socket" or simply "system
   socket") name is set to ``/dev/log``. Otherwise, if the user `has`
   explicitly set ``SysSock.Use="off"``, then rsyslog will not listen on
   ``/dev/log`` OR any socket defined by the ``SysSock.Name`` parameter and
   the rest of this section does not apply.

#. If the user has specified ``sysSock.Name="/path/to/custom/socket"`` (and not
   explicitly set ``SysSock.Use="off"``), then the default listener socket name
   is overwritten with ``/path/to/custom/socket``.

#. Otherwise, if rsyslog is running under systemd AND
   ``/run/systemd/journal/syslog`` exists, (AND the user has not
   explicitly set ``SysSock.Use="off"``) then the default listener socket name
   is overwritten with ``/run/systemd/journal/syslog``.


Step 2: Listen on specified sockets
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::

   This is true for all sockets, be it system socket or not. But if
   ``SysSock.Use="off"``, the system socket will not be listened on.

rsyslog evaluates the list of sockets it has been asked to activate:

- the system log socket (if still enabled after completion of the last section)
- any custom inputs defined by the user

and then checks to see if it has been passed in via systemd (name is checked).
If it was passed in via systemd, the socket is used as-is (e.g., not recreated
upon rsyslog startup), otherwise if not passed in via systemd the log socket
is unlinked, created and opened.


Step 3: Shutdown log sockets
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::

   This is true for all sockets, be it system socket or not.

Upon shutdown, rsyslog processes each socket it is listening on and evaluates
it. If the socket was originally passed in via systemd (name is checked), then
rsyslog does nothing with the socket (systemd maintains the socket).

If the socket was `not` passed in via systemd AND the configuration permits
rsyslog to do so (the default setting), rsyslog will unlink/remove the log
socket. If not permitted to do so (the user specified otherwise), then rsyslog
will not unlink the log socket and will leave that cleanup step to the
user or application that created the socket to remove it.


Statistic Counter
=================

This plugin maintains a global :doc:`statistics <../rsyslog_statistic_counter>` with the following properties:

- ``submitted`` - total number of messages submitted for processing since startup

- ``ratelimit.discarded`` - number of messages discarded due to rate limiting

- ``ratelimit.numratelimiters`` - number of currently active rate limiters
  (small data structures used for the rate limiting logic)


Caveats/Known Bugs
==================

- When running under systemd, **many "sysSock." parameters are ignored**.
  See parameter descriptions and the :ref:`imuxsock-systemd-details-label` section for
  details.

- On systems where systemd is used this module is often not loaded by default.
  See the :ref:`imuxsock-systemd-details-label` section for details.

- Application timestamps are ignored by default. See the
  :ref:`imuxsock-application-timestamps-label` section for details.

- `imuxsock does not work on Solaris
  <http://www.rsyslog.com/why-does-imuxsock-not-work-on-solaris/>`_

.. todolist::


Examples
========

Minimum setup
-------------

The following sample is the minimum setup required to accept syslog
messages from applications running on the local system.

.. code-block:: none

   module(load="imuxsock")

This only needs to be done once.


Enable flow control
-------------------

.. code-block:: none
  :emphasize-lines: 2

   module(load="imuxsock" # needs to be done just once
          SysSock.FlowControl="on") # enable flow control (use if needed)

Enable trusted properties
-------------------------

As noted in the :ref:`imuxsock-trusted-properties-label` section, trusted properties
are disabled by default. If you want to use them, you must turn the feature
on via ``SysSock.Annotate`` for the system log socket and ``Annotate`` for
inputs.

Append to end of message
^^^^^^^^^^^^^^^^^^^^^^^^

The following sample is used to activate message annotation and thus
trusted properties on the system log socket. These trusted properties
are appended to the end of each message.

.. code-block:: none
   :emphasize-lines: 2

   module(load="imuxsock" # needs to be done just once
            SysSock.Annotate="on")


Store in JSON message properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following sample is similar to the first one, but enables parsing of
trusted properties, which places the results into JSON/lumberjack variables.

.. code-block:: none
   :emphasize-lines: 2

   module(load="imuxsock"
            SysSock.Annotate="on" SysSock.ParseTrusted="on")

Read log data from jails
------------------------

The following sample is a configuration where rsyslogd pulls logs from
two jails, and assigns different hostnames to each of the jails:

.. code-block:: none
   :emphasize-lines: 3,4,5

   module(load="imuxsock") # needs to be done just once
   input(type="imuxsock"
         HostName="jail1.example.net"
         Socket="/jail/1/dev/log") input(type="imuxsock"
         HostName="jail2.example.net" Socket="/jail/2/dev/log")

Read from socket on temporary file system
-----------------------------------------

The following sample is a configuration where rsyslogd reads the openssh
log messages via a separate socket, but this socket is created on a
temporary file system. As rsyslogd starts up before the sshd daemon, it needs
to create the socket directories, because it otherwise can not open the
socket and thus not listen to openssh messages.

.. code-block:: none
   :emphasize-lines: 3,4

   module(load="imuxsock") # needs to be done just once
   input(type="imuxsock"
         Socket="/var/run/sshd/dev/log"
         CreatePath="on")


Disable rate limiting
---------------------

The following sample is used to turn off input rate limiting on the
system log socket.

.. code-block:: none
   :emphasize-lines: 2

   module(load="imuxsock" # needs to be done just once
            SysSock.RateLimit.Interval="0") # turn off rate limiting


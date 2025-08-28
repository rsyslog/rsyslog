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

   Parameter names are case-insensitive; camelCase is recommended for readability.

Module Parameters
-----------------

.. warning::

   When running under systemd, **many "sysSock." parameters are ignored**.
   See parameter descriptions and the :ref:`imuxsock-systemd-details-label` section for
   details.


.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imuxsock-syssock-ignoretimestamp`
     - .. include:: ../../reference/parameters/imuxsock-syssock-ignoretimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-ignoreownmessages`
     - .. include:: ../../reference/parameters/imuxsock-syssock-ignoreownmessages.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-use`
     - .. include:: ../../reference/parameters/imuxsock-syssock-use.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-name`
     - .. include:: ../../reference/parameters/imuxsock-syssock-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-flowcontrol`
     - .. include:: ../../reference/parameters/imuxsock-syssock-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-usepidfromsystem`
     - .. include:: ../../reference/parameters/imuxsock-syssock-usepidfromsystem.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-ratelimit-interval`
     - .. include:: ../../reference/parameters/imuxsock-syssock-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-ratelimit-burst`
     - .. include:: ../../reference/parameters/imuxsock-syssock-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-ratelimit-severity`
     - .. include:: ../../reference/parameters/imuxsock-syssock-ratelimit-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-usesystimestamp`
     - .. include:: ../../reference/parameters/imuxsock-syssock-usesystimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-annotate`
     - .. include:: ../../reference/parameters/imuxsock-syssock-annotate.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-parsetrusted`
     - .. include:: ../../reference/parameters/imuxsock-syssock-parsetrusted.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-unlink`
     - .. include:: ../../reference/parameters/imuxsock-syssock-unlink.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-usespecialparser`
     - .. include:: ../../reference/parameters/imuxsock-syssock-usespecialparser.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-syssock-parsehostname`
     - .. include:: ../../reference/parameters/imuxsock-syssock-parsehostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imuxsock-ruleset`
     - .. include:: ../../reference/parameters/imuxsock-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-ignoretimestamp`
     - .. include:: ../../reference/parameters/imuxsock-ignoretimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-ignoreownmessages`
     - .. include:: ../../reference/parameters/imuxsock-ignoreownmessages.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-flowcontrol`
     - .. include:: ../../reference/parameters/imuxsock-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-ratelimit-interval`
     - .. include:: ../../reference/parameters/imuxsock-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-ratelimit-burst`
     - .. include:: ../../reference/parameters/imuxsock-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-ratelimit-severity`
     - .. include:: ../../reference/parameters/imuxsock-ratelimit-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-usepidfromsystem`
     - .. include:: ../../reference/parameters/imuxsock-usepidfromsystem.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-usesystimestamp`
     - .. include:: ../../reference/parameters/imuxsock-usesystimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-createpath`
     - .. include:: ../../reference/parameters/imuxsock-createpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-socket`
     - .. include:: ../../reference/parameters/imuxsock-socket.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-hostname`
     - .. include:: ../../reference/parameters/imuxsock-hostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-annotate`
     - .. include:: ../../reference/parameters/imuxsock-annotate.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-parsetrusted`
     - .. include:: ../../reference/parameters/imuxsock-parsetrusted.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-unlink`
     - .. include:: ../../reference/parameters/imuxsock-unlink.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-usespecialparser`
     - .. include:: ../../reference/parameters/imuxsock-usespecialparser.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imuxsock-parsehostname`
     - .. include:: ../../reference/parameters/imuxsock-parsehostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

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


.. toctree::
   :hidden:

   ../../reference/parameters/imuxsock-syssock-ignoretimestamp
   ../../reference/parameters/imuxsock-syssock-ignoreownmessages
   ../../reference/parameters/imuxsock-syssock-use
   ../../reference/parameters/imuxsock-syssock-name
   ../../reference/parameters/imuxsock-syssock-flowcontrol
   ../../reference/parameters/imuxsock-syssock-usepidfromsystem
   ../../reference/parameters/imuxsock-syssock-ratelimit-interval
   ../../reference/parameters/imuxsock-syssock-ratelimit-burst
   ../../reference/parameters/imuxsock-syssock-ratelimit-severity
   ../../reference/parameters/imuxsock-syssock-usesystimestamp
   ../../reference/parameters/imuxsock-syssock-annotate
   ../../reference/parameters/imuxsock-syssock-parsetrusted
   ../../reference/parameters/imuxsock-syssock-unlink
   ../../reference/parameters/imuxsock-syssock-usespecialparser
   ../../reference/parameters/imuxsock-syssock-parsehostname
   ../../reference/parameters/imuxsock-ruleset
   ../../reference/parameters/imuxsock-ignoretimestamp
   ../../reference/parameters/imuxsock-ignoreownmessages
   ../../reference/parameters/imuxsock-flowcontrol
   ../../reference/parameters/imuxsock-ratelimit-interval
   ../../reference/parameters/imuxsock-ratelimit-burst
   ../../reference/parameters/imuxsock-ratelimit-severity
   ../../reference/parameters/imuxsock-usepidfromsystem
   ../../reference/parameters/imuxsock-usesystimestamp
   ../../reference/parameters/imuxsock-createpath
   ../../reference/parameters/imuxsock-socket
   ../../reference/parameters/imuxsock-hostname
   ../../reference/parameters/imuxsock-annotate
   ../../reference/parameters/imuxsock-parsetrusted
   ../../reference/parameters/imuxsock-unlink
   ../../reference/parameters/imuxsock-usespecialparser
   ../../reference/parameters/imuxsock-parsehostname


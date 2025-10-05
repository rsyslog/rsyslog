Reliable Forwarding of syslog Messages with Rsyslog
===================================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_
*(2008-06-27)*

Abstract
--------

**In this paper, I describe how to forward**
`syslog <http://www.monitorware.com/en/topics/syslog/>`_ **messages
(quite) reliable to a central rsyslog server.** This depends on rsyslog
being installed on the client system and it is recommended to have it
installed on the server system. Please note that industry-standard
`plain TCP syslog protocol is not fully
reliable <https://rainer.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html>`_
(thus the "quite reliable"). If you need a truly reliable solution, you
need to look into RELP (natively supported by rsyslog).*

The Intention
-------------

Whenever two systems talk over a network, something can go wrong. For
example, the communications link may go down, or a client or server may
abort. Even in regular cases, the server may be offline for a short
period of time because of routine maintenance.

A logging system should be capable of avoiding message loss in
situations where the server is not reachable. To do so, unsent data
needs to be buffered at the client while the server is offline. Then,
once the server is up again, this data is to be sent.

This can easily be accomplished by rsyslog. In rsyslog, every action runs
on its own queue and each queue can be set to buffer data if the action
is not ready. Of course, you must be able to detect that "the action is
not ready", which means the remote server is offline. This can be
detected with plain TCP syslog and RELP, but not with UDP. So you need
to use either of the two. In this howto, we use plain TCP syslog.

Please note that we are using rsyslog-specific features. They are
required on the client, but not on the server. So the client system must
run rsyslog (at least version 3.12.0), while on the server another
syslogd may be running, as long as it supports plain tcp syslog.

**The rsyslog queueing subsystem tries to buffer to memory. So even if
the remote server goes offline, no disk file is generated.** File on
disk are created only if there is need to, for example if rsyslog runs
out of (configured) memory queue space or needs to shutdown (and thus
persist yet unsent messages). Using main memory and going to the disk
when needed is a huge performance benefit. You do not need to care about
it, because, all of it is handled automatically and transparently by
rsyslog.

How To Setup
------------

First, you need to create a working directory for rsyslog. This is where
it stores its queue files (should need arise). You may use any location
on your local system.

Next, you need to instruct rsyslog to use a disk queue and then
configure your action. There is nothing else to do. With the following
simple config file, you forward anything you receive to a remote server
and have buffering applied automatically when it goes down. This must be
done on the client machine.

.. code-block:: rsyslog

    module(load="imuxsock")            # local message reception
    global(workDirectory="/rsyslog/work")

    action(
        type="omfwd"
        target="server"
        port="port"                     # optional
        protocol="tcp"
        action.resumeRetryCount="-1"     # infinite retries on insert failure
        queue.type="LinkedList"          # use asynchronous processing
        queue.filename="srvrfwd"         # enables disk assistance
        queue.saveOnShutdown="on"        # persist data if rsyslog shuts down
    )

The port given above is optional. It may be omitted, in which case you
only provide the server name. The ``queue.filename`` is used to create
disk-assisted queue files, should need arise. This value must be unique
inside the configuration. No two actions must use the same queue file.
Also, for obvious reasons, it must only contain those characters that
can be used inside a valid file name. Rsyslog possibly adds some
characters in front and/or at the end of that name when it creates
files. So that name should not be at the file size name length limit
(which should not be a problem these days). See
:doc:`../rainerscript/queue_parameters` for the full list of queue
options.

Please note that actual spool files are only created if the remote
server is down **and** there is no more space in the in-memory queue. By
default, a short failure of the remote server will never result in the
creation of a disk file as a couple of hundred messages can be held in
memory by default. [These parameters can be fine-tuned. However, then
you need to either fully understand how the queue works (`read elaborate
doc <http://www.rsyslog.com/doc-queues.html>`_) or use `professional
services <http://www.rsyslog.com/professional-services/>`_ to
have it done based on your specs ;) - what that means is that
fine-tuning queue parameters is far from being trivial...]

If you would like to test if your buffering scenario works, you need to
stop, wait a while and restart your central server. Do **not** watch for
files being created, as this usually does not happen and never happens
immediately.

Forwarding to More than One Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you have more than one server you would like to forward to, that's
quickly done. Rsyslog has no limit on the number or type of actions, so
you can define as many targets as you like. What is important to know,
however, is that the full set of directives make up an action. So you
can not simply add (just) a second forwarding rule, but need to
duplicate the rule configuration as well. Be careful that you use
different queue file names for the second action, else you will mess up
your system.

A sample for forwarding to two hosts looks like this:

.. code-block:: rsyslog

    module(load="imuxsock")
    global(workDirectory="/rsyslog/work")

    # start forwarding rule 1
    action(
        type="omfwd"
        target="server1"
        port="port"
        protocol="tcp"
        action.resumeRetryCount="-1"
        queue.type="LinkedList"
        queue.filename="srvrfwd1"
        queue.saveOnShutdown="on"
    )
    # end forwarding rule 1

    # start forwarding rule 2
    action(
        type="omfwd"
        target="server2"
        protocol="tcp"
        action.resumeRetryCount="-1"
        queue.type="LinkedList"
        queue.filename="srvrfwd2"
        queue.saveOnShutdown="on"
    )
    # end forwarding rule 2

Note the filename used for the first rule it is ``srvrfwd1`` and for the
second it is ``srvrfwd2``. I have used a server without port name in the
second forwarding rule. This was just to illustrate how this can be
done. You can also specify a port there (or drop the port from
``server1``).

When there are multiple action queues, they all work independently.
Thus, if server1 goes down, server2 still receives data in real-time.
The client will **not** block and wait for server1 to come back online.
Similarly, server1's operation will not be affected by server2's state.

See Also
--------

- :doc:`high_database_rate` for a more in-depth discussion of tuning
  disk-assisted queues when writing to slow destinations such as
  databases.
- :doc:`failover_syslog_server` for a pattern that combines reliable
  forwarding with multiple fallback targets.
- :doc:`../concepts/queues` for a conceptual overview of the queueing
  subsystem that underpins these configurations.

Some Final Words on Reliability ...
-----------------------------------

Using plain TCP syslog provides a lot of reliability over UDP syslog.
However, plain TCP syslog is **not** a fully reliable transport. In
order to get full reliability, you need to use the RELP protocol.

Follow the next link to learn more about `the problems you may encounter
with plain tcp
syslog <https://rainer.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html>`_.

Feedback requested
~~~~~~~~~~~~~~~~~~

I would appreciate feedback on this tutorial. If you have additional
ideas, comments or find bugs (I \*do\* bugs - no way... ;)), please `let
me know <mailto:rgerhards@adiscon.com>`_.

Revision History
----------------

-  2008-06-27 \* `Rainer Gerhards <https://rainer.gerhards.net/>`_ \*
   Initial Version created


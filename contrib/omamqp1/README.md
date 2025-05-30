# AMQP 1.0 Output Module #

The omamqp1 output module can be used to send log messages via an AMQP
1.0-compatible messaging bus.

This module requires the Apache QPID Proton python library, version
0.10+.  This should be installed on the system that is running
rsyslogd.

## Message Format ##

Messages sent from this module to the message bus contain a list of
strings.  Each string is a separate log message.  The list is ordered
such that the oldest log appears at the front of the list, whilst the
most recent log is at the end of the list.

## Configuration ##

This module is configured via the rsyslog.conf configuration file.  To
use this module it must first be imported.

Example:

   module(load="omamqp1")

Actions can then be created using this module.

Example:

    action(type="omamqp1"
           host="localhost:5672"
           target="amq.topic")

The following parameters are recognized by the module:

* host - The address of the message bus.  Optionally a port can be
  included, separated by a ':'.  Example: "localhost:5672"
* target - The destination for the generated messages.  This can be
  the name of a queue or topic.  On some messages buses it may be
  necessary to create this target manually.  Example: "amq.topic"
* username - Optional.  Used by SASL to authenticate with the message bus.
* password - Optional.  Used by SASL to authenticate with the message bus.
* template - Template to use to format the record.
  Defaults to `RSYSLOG_FileFormat`
* idleTimeout - The idle timeout in seconds.  This enables connection
  heartbeats and is used to detect a failed connection to the message
  bus.  Set to zero to disable.
* maxResend - number of times an undeliverable message is re-sent to
  the message bus before it is dropped. This is unrelated to rsyslog's
  action.resumeRetryCount.  Once the connection to the message bus is
  active this module is ready to receive log messages from rsyslog
  (i.e. the module has 'resumed').  Even though the connection is
  active, any particular message may be rejected by the message bus
  (e.g. 'unrouteable').  The module will retry (e.g. 'suspend') for up
  to maxResend attempts before discarding the message as
  undeliverable.  Setting this to zero disables the limit and
  unrouteable messages will be retried as long as the connection stays
  up.  You probably do not want that to happen.  The default is 10.
* reconnectDelay - The time in seconds this module will delay before
  attempting to re-established a failed connection (default 5
  seconds).
* disableSASL - Setting this to a non-zero value will disable SASL
  negotiation.  Only necessary if the message bus does not offer SASL.

## Dependencies ##

The package is dependent on the QPID Proton AMQP 1.0 library.

To build this package you must also have the QPID Proton C headers
installed.

Pre-built packages are available for Fedora 22+ via the base Fedora
repos.  Packages for RHEL/Centos based systems can be obtained via
[EPEL](https://fedoraproject.org/wiki/EPEL).  In order to build the
module, install the _qpid-proton-c-devel_ package.

Pre-built packages from most Ubuntu/Debian systems are available via
the [QPID project's PPA on Launchpad](https://launchpad.net/~qpid).
For example, to install the latest version of the Proton packages (as
of this writing):

  $ sudo add-apt-repository ppa:qpid/released
  $ sudo apt-get update
  $ sudo apt-get install libqpid-proton3 libqpid-proton3-dev

Check your distribution for the availability of Proton
packages.  Alternatively, you can pull down the Proton code from the
[project website](http://qpid.apache.org/) and build it yourself.


## Debugging ##

Debug logging can be enabled using the environment variables
`RSYSLOG_DEBUG` and `RSYSLOG_DEBUGLOG`,

    export RSYSLOG_DEBUG=debug
    export RSYSLOG_DEBUGLOG=/tmp/rsyslog.debug.log

or with the old-style rsyslog debug configuration settings.  For example:

    $DebugFile /tmp/omamqp1-debug.txt
    $DebugLevel 2

There are a number of tracepoints within the omamqp1.c code.

----

## Notes on use with the QPID C++ broker (qpidd) ##

_Note well: These notes assume use of version 0.34 of the QPID C++
broker. Previous versions may not be fully compatible_

To use the Apache QPID C++ broker _qpidd_ as the message bus, a
version of qpidd that supports the AMQP 1.0 protocol must be used.

Since qpidd can be packaged without AMQP 1.0 support you should verify
AMQP 1.0 has been enabled by checking for AMQP 1.0 related options in
the qpidd help text.  For example:

    qpidd --help

    ...

    AMQP 1.0 Options:
      --domain DOMAIN           Domain of this broker
      --queue-patterns PATTERN  Pattern for on-demand queues
      --topic-patterns PATTERN  Pattern for on-demand topics
   
If no AMQP 1.0 related options appear in the help output, then AMQP
1.0 has not been included with your qpidd.

The destination for message (target) must be created before log
messages arrive.  This can be done using the qpid-config tool.

Example:

    qpid-config add queue rsyslogd

Alternatively, the target can be created on demand by configuring a
queue-pattern (or topic-pattern) that matches the target.  To do this,
add a _queue-patterns_ (or _topic_patterns_) directive to the qpidd
configuration file /etc/qpid/qpidd.conf.

For example, to have qpidd automatically create a queue named
_rsyslogd_, add the following to the qpidd configuration file:

    queue-patterns=rsyslogd

or, if a topic is desired instead of a queue:

    topic-patterns=rsyslogd

These dynamic targets are auto-delete and will be destroyed once there
are no longer any subscribers or queue-bound messages.

Versions of qpidd <= 0.34 also need to have the SASL service name set
to 'amqp'. Add this to the qpidd.conf file:

    sasl-service-name=amqp

----

## Notes on use with the QPID Dispatch Router (qdrouterd) ##

_Note well: These notes assume use of version 0.5 of the QPID Dispatch
Router Previous versions may not be fully compatible_


The default qdrouterd configuration does not have SASL authentication
turned on.  You must set up SASL in the qdrouter configuration file
/etc/qpid-dispatch/qdrouterd.conf

First create a SASL configuration file for qdrouterd.  This
configuration file is usually /etc/sasl2/qdrouterd.conf, but its
default location may vary depending on your platform's configuration.

This document assumes you understand how to properly configure SASL.

Here is an example qdrouterd SASL configuration file that allows the
client to use the DIGEST-MD5 or PLAIN authentication mechanisms, plus
a SASL user database:

    pwcheck_method: auxprop
    auxprop_plugin: sasldb
    sasldb_path: /var/lib/qdrouterd/qdrouterd.sasldb
    mech_list: DIGEST-MD5 PLAIN

Once a SASL configuration file has been set up for qdrouterd the path
to the directory holding the configuration file and the basename of
the configuration file (sas '.conf') must be added to the
/etc/qpid-dispatch/qdrouterd.conf configuration file.  This is done by
adding _saslConfigPath_ and _saslConfigName_ to the _container_
section of the configuration file. For example, assuming the file
/etc/sasl2/qdrouter.conf holds the qdrouterd SASL configuration:

    container {
        workerThreads: 4
        containerName: Qpid.Dispatch.Router.A
        saslConfigPath: /etc/sasl2
        saslConfigName: qdrouterd
    }

In addition, the address used by the omamqp1 module to connect to
qdrouterd must have SASL authentication turned on.  This is done by
adding the _authenticatePeer_ attribute set to 'yes' to the
corresponding _listener_ entry:

    listener {
        addr: 0.0.0.0
        port: amqp
        authenticatePeer: yes 
    }

This should complete the SASL setup needed by qdrouterd.

The target address used as the destination for the log messages must
be picked with care.  qdrouterd uses the prefix of the target address
to determine the forwarding pattern used for messages sent using that
target address.  Addresses starting with the prefix _queue_ are
distributed to only one message receiver.  If there are multiple
message consumers listening to that target address, only one listener
will receive the message.  In this case, qdrouterd will load balance
messages across the multiple consumers - much like a queue with
competing subscribers. For example: "queue/rsyslogd"

If a multicast pattern is desired - where all active listeners receive
their own copy of the message - the target address prefix _multicast_
may be used.  For example: "multicast/rsyslogd"

Note well: if there are _no_ active receivers for the log messages,
messages will be rejected the qdrouterd.  In this case the omamqp1
module will return a _SUSPENDED_ result to the rsyslogd main task.
rsyslogd may then re-submit the rejected log messages to the module,
which will attempt to send them again.  This retry option is
configured via rsyslogd - it is not part of this module.  Refer to the
rsyslogd actions documentation.

----

### Using qdrouterd in combination with qpidd ###

A qdrouterd-based message bus can use a broker as a message storage
mechanism for those that require broker-based message services (such
as a message store).  This section explains how to configure qdrouterd
and qpidd for this type of deployment.  Please read the notes for
deploying qpidd and qdrouterd first.

Each qdrouterd instance that is to connect the broker to the message
bus must define a _connector_ section in the qdrouterd.conf file.
This connector contains the addressing information necessary to have
the message bus set up a connection to the broker.  For example, if a
broker is available on host broker.host.com at port 5672:

    connector {
        name: mybroker
        role: on-demand
        addr: broker.host.com
        port: 5672
    }

In order to route messages to and from the broker, a static _link
route_ must be configured on qdrouterd.  This link route contains a
target address prefix and the name of the connector to use for
forwarding matching messages.

For example, to have qdrouterd forward messages that have a target
address prefixed by 'Broker' to the connector defined above, the
following link pattern must be added to the qdrouterd.conf
configuration:

    linkRoutePattern {
        prefix: /Broker/
        connector: mybroker
    }

A queue must then be created on the broker.  The name of the queue
must be prefixed by the same prefix specified in the linkRoutePattern
entry.  For example:

    $ qpid-config add queue Broker/rsyslogd

Lastly, use the name of the queue for the target address used by the
omamqp module.  For example, assuming qdrouterd is listening on local
port 5672:

    action(type="omamqp1"
           host="localhost:5672"
           target="Broker/rsyslogd")

# Tests

## Valgrind
For investigating leaks, valgrind will not give you the full symbols
in the stack because rsyslog unloads the module before the leak checker
finishes.  The test adds the following using `add_conf`

	    add_conf '
    global(debug.unloadModules="off")
    '

Then you should get a full stacktrace with full symbols.

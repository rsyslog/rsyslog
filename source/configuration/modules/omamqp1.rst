omamqp1: AMQP 1.0 Messaging Output Module
=========================================

===========================  ===========================================================================
**Module Name:**             **omamqp1**
**Available Since:**         **8.17.0**
**Original Author:**         Ken Giusti <kgiusti@gmail.com>
===========================  ===========================================================================

**Description**

This module provides the ability to send logging via an AMQP 1.0
compliant message bus.  It puts the log messages into an AMQP
message and sends the message to a destination on the bus.

**Dependencies**

* `libqpid-proton <http://qpid.apache.org/proton>`_

**Configure**
::

    ./configure --enable-omamqp1

**Message Format**

Messages sent from this module to the message bus contain an AMQP List
in the message body.  This list contains one or more log messages as
AMQP String types.  Each string entry is a single log message.  The
list is ordered such that the oldest log appears at the front of the
list (e.g. list index 0), whilst the most recent log is at the end of
the list.

**Interoperability**

The output plugin has been tested against the following messaging systems:

* `QPID C++ Message Broker <http://qpid.apache.org/components/cpp-broker>`_
* `QPID Dispatch Message Router <http://qpid.apache.org/components/dispatch-router>`_

**Action Parameters**

Note: parameter names are case-insensitive.

-  **host**
   *(Required)* The address of the message bus in *host[:port]* format.
   The port defaults to 5672 if absent. Examples: *"localhost"*,
   *"127.0.0.1:9999"*, *"bus.someplace.org"*

-  **target**
   *(Required)* The destination for the generated messages.  This can be
   the name of a queue or topic.  On some messages buses it may be
   necessary to create this target manually.  Example: *"amq.topic"*

-  **username**
   *(Optional)* Used by SASL to authenticate with the message bus.

-  **password**
   *(Optional)* Used by SASL to authenticate with the message bus.

-  **template**
   *(Optional)* Format for the log messages. Default: *RSYSLOG_FileFormat*

-  **idleTimeout**
   *(Optional)* The idle timeout in seconds.  This enables connection
   heartbeats and is used to detect a failed connection to the message
   bus.  Set to zero to disable. Default: *0*

-  **reconnectDelay**
   *(Optional)* The time in seconds this module will delay before
   attempting to re-established a failed connection to the message bus. Default: *5*

-  **maxRetries**
   *(Optional)* The number of times an undeliverable message is
   re-sent to the message bus before it is dropped. This is unrelated
   to rsyslog's action.resumeRetryCount.  Once the connection to the
   message bus is active this module is ready to receive log messages
   from rsyslog (i.e. the module has 'resumed').  Even though the
   connection is active, any particular message may be rejected by the
   message bus (e.g. 'unrouteable').  The module will retry
   (e.g. 'suspend') for up to *maxRetries* attempts before discarding
   the message as undeliverable.  Setting this to zero disables the
   limit and unrouteable messages will be retried as long as the
   connection stays up.  You probably do not want that to
   happen. Default: *10*

-  **disableSASL**
   *(Optional)* Setting this to a non-zero value will disable SASL
   negotiation.  Only necessary if the message bus does not offer SASL
   support.


**TODO**

-  Add support for SSL connections.

**Examples**

This example shows a minimal configuration.  The module will attempt
to connect to a QPID broker at *broker.amqp.org*.  Messages are
sent to the *amq.topic* topic, which exists on the broker by default:

::

   module(load="omamqp1")
   action(type="omamqp1"
          host="broker.amqp.org"
          target="amq.topic")

This example forces rsyslogd to authenticate with the message bus.
The message bus must be provisioned such that user *joe* is allowed to
send to the message bus.  All messages are sent to *log-queue*.  It is
assumed that *log-queue* has already been provisioned:

::

   module(load="omamqp1")

   action(type="omamqp1"
          host="bus.amqp.org"
          target="log-queue"
          username="joe"
          password="trustno1")

-------------------------------------------------
**Notes on use with the QPID C++ broker (qpidd)**
-------------------------------------------------

*Note well*: These notes assume use of version 0.34 of the QPID C++
broker. Previous versions may not be fully compatible.

To use the Apache QPID C++ broker **qpidd** as the message bus, a
version of qpidd that supports the AMQP 1.0 protocol must be used.

Since qpidd can be packaged without AMQP 1.0 support you should verify
AMQP 1.0 has been enabled by checking for AMQP 1.0 related options in
the qpidd help text.  For example:

::

   qpidd --help

   ...

   AMQP 1.0 Options:
      --domain DOMAIN           Domain of this broker
      --queue-patterns PATTERN  Pattern for on-demand queues
      --topic-patterns PATTERN  Pattern for on-demand topics


If no AMQP 1.0 related options appear in the help output then your
instance of qpidd does not support AMQP 1.0 and cannot be used with
this output module.

The destination for message (target) *must* be created before log
messages arrive.  This can be done using the qpid-config tool.

Example:

::

   qpid-config add queue rsyslogd

Alternatively the target can be created on demand by configuring a
queue-pattern (or topic-pattern) that matches the target.  To do this,
add a *queue-patterns* or *topic_patterns* configuration directive to
the qpidd configuration file /etc/qpid/qpidd.conf.

For example to have qpidd automatically create a queue named
*rsyslogd* add the following to the qpidd configuration file:

::

   queue-patterns=rsyslogd

or, if a topic behavior is desired instead of a queue:

::

   topic-patterns=rsyslogd

These dynamic targets are auto-delete and will be destroyed once there
are no longer any subscribers or queue-bound messages.

Versions of qpidd <= 0.34 also need to have the SASL service name set
to *"amqp"* if SASL authentication is used. Add this to the qpidd.conf
file:

::

   sasl-service-name=amqp


----------------------------------------------------------
**Notes on use with the QPID Dispatch Router (qdrouterd)**
----------------------------------------------------------

*Note well*: These notes assume use of version 0.5 of the QPID Dispatch
Router **qdrouterd**. Previous versions may not be fully compatible.

The default qdrouterd configuration does not have SASL authentication
turned on.  If SASL authentication is required you must configure SASL
in the qdrouter configuration file /etc/qpid-dispatch/qdrouterd.conf

First create a SASL configuration file for qdrouterd.  This
configuration file is usually /etc/sasl2/qdrouterd.conf, but its
default location may vary depending on your platform's configuration.

This document assumes you understand how to properly configure Cyrus
SASL.

Here is an example qdrouterd SASL configuration file that allows the
client to use either the **DIGEST-MD5** or **PLAIN** authentication
mechanisims and specifies the path to the SASL user credentials
database:

::

   pwcheck_method: auxprop
   auxprop_plugin: sasldb
   sasldb_path: /var/lib/qdrouterd/qdrouterd.sasldb
   mech_list: DIGEST-MD5 PLAIN

Once a SASL configuration file has been set up for qdrouterd the path
to the directory holding the configuration file and the name of the
configuration file itself **without the '.conf' suffix** must be added
to the /etc/qpid-dispatch/qdrouterd.conf configuration file.  This is
done by adding *saslConfigPath* and *saslConfigName* to the
*container* section of the configuration file. For example, assuming
the file /etc/sasl2/qdrouterd.conf holds the qdrouterd SASL
configuration:

::

   container {
      workerThreads: 4
      containerName: Qpid.Dispatch.Router.A
      saslConfigPath: /etc/sasl2
      saslConfigName: qdrouterd
   }


In addition the address used by the omamqp1 module to connect to
qdrouterd must have SASL authentication turned on.  This is done by
adding the *authenticatePeer* attribute set to 'yes' to the
corresponding *listener* entry:

::

   listener {
      addr: 0.0.0.0
      port: amqp
      authenticatePeer: yes
   }

This should complete the SASL setup needed by qdrouterd.

The target address used as the destination for the log messages must
be picked with care.  qdrouterd uses the prefix of the target address
to determine the forwarding pattern used for messages sent to that
target address.  Addresses starting with the prefix *queue* are
distributed to only one message receiver.  If there are multiple
message consumers listening to that target address only one listener
will receive the message - mimicking the behavior of a queue with
competing subscribers. For example: *queue/rsyslogd*

If a multicast pattern is desired - where all active listeners receive
their own copy of the message - the target address prefix *multicast*
may be used.  For example: *multicast/rsyslogd*

Note well: if there are no active receivers for the log messages the
messages will be rejected by qdrouterd since the messages are
undeliverable.  In this case the omamqp1 module will return a
**SUSPENDED** status to the rsyslogd main task.  rsyslogd may then
re-submit the rejected log messages to the module which will attempt
to send them again.  This retry option is configured via rsyslogd - it
is not part of this module.  Refer to the rsyslogd actions
documentation.


---------------------------------------------
**Using qdrouterd in combination with qpidd**
---------------------------------------------

A qdrouterd-based message bus can use a broker as a message storage
mechanism for those that require broker-based message services (such
as a message store).  This section explains how to configure qdrouterd
and qpidd for this type of deployment.  Please read the above notes
for deploying qpidd and qdrouterd first.

Each qdrouterd instance that is to connect the broker to the message
bus must define a *connector* section in the qdrouterd.conf file.
This connector contains the addressing information necessary to have
the message bus set up a connection to the broker.  For example, if a
broker is available on host broker.host.com at port 5672:

::

   connector {
      name: mybroker
      role: on-demand
      addr: broker.host.com
      port: 5672
   }

In order to route messages to and from the broker, a static *link
route* must be configured on qdrouterd.  This link route contains a
target address prefix and the name of the connector to use for
forwarding matching messages.

For example, to have qdrouterd forward messages that have a target
address prefixed by "Broker" to the connector defined above, the
following link pattern must be added to the qdrouterd.conf
configuration:

::

   linkRoutePattern {
      prefix: /Broker/
      connector: mybroker
   }

A queue must then be created on the broker.  The name of the queue
must be prefixed by the same prefix specified in the linkRoutePattern
entry.  For example:

::

   $ qpid-config add queue Broker/rsyslogd

Lastly use the name of the queue for the target address for the omamqp
module action.  For example, assuming qdrouterd is listening on local
port 5672:

::

   action(type="omamqp1"
          host="localhost:5672"
          target="Broker/rsyslogd")


This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008-2016 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.

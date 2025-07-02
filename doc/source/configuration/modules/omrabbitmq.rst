**********************************
omrabbitmq: RabbitMQ output module
**********************************

===========================  ===========================================================================
**Module Name:**             **omrabbitmq**
**Authors:**                 Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> / Philippe Duveau <philippe.duveau@free.fr> / Hamid Maadani <hamid@dexo.tech>
===========================  ===========================================================================


Purpose
=======

This module sends syslog messages into RabbitMQ server.
Only v6 configuration syntax is supported.

**omrabbitmq is tested and is running in production with 8.x version of rsyslog.**

Compile
=======

To successfully compile omrabbitmq module you need `rabbitmq-c <https://github.com/alanxz/rabbitmq-c>`_ library version >= 0.4.

    ./configure --enable-omrabbitmq ...

Configuration Parameters
========================

Action Parameters
-----------------

host
^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "hostname\[:port\]\[ hostname2\[:port2\]\]",

rabbitmq server(s). See HA configuration

port
^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "port", "5672"

virtual\_host
^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "path",

virtual message broker

user
^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "user",

user name

password
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "password",

user password

ssl
^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", , "off"

enable TLS for AMQP connection

init_openssl
^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", , "off"

should rabbitmq-c initialize OpenSSL? This is included to prevent crashes caused by OpenSSL double initialization. Should stay off in most cases. ONLY turn on if SSL does not work due to OpenSSL not being initialized.

verify_peer
^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", , "off"

SSL peer verification

verify_hostname
^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", , "off"

SSL certificate hostname verification

ca_cert
^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "file path",

CA certificate to be used for the SSL connection

heartbeat_interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "integer", "no", , "0"

AMQP heartbeat interval in seconds. 0 means disabled, which is default.

exchange
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "name",

exchange name

routing\_key
^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "name",

value of routing key

routing\_key\_template
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "template_name", 

template used to compute the routing key

body\_template
^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "template", "StdJSONFmt"

template used to compute the message body. If the template is an empty string the sent message will be %rawmsg%

delivery\_mode
^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "TRANSIENT\|PERSISTANT", "TRANSIENT"

persistence of the message in the broker

expiration
^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "milliseconds", no expiration

ttl of the amqp message

populate\_properties
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", , "off"

fill timestamp, appid, msgid, hostname (custom header) with message information

content\_type
^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "value", 

content type as a MIME value

declare\_exchange
^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", "off", 

Rsyslog tries to declare the exchange on startup. Declaration failure (already exists with different parameters or insufficient rights) is warned but does not cancel the module instance.

recover\_policy
^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "check\_interval;short\_failure_interval; short\_failure\_nb\_max;graceful\_interval", "60;6;3;600"

See HA configuration

HA configuration
================

The module can use two rabbitmq server in a fail-over mode. To configure this mode, the host parameter has to reference the two rabbitmq servers separated by space.
Each server can be optionally completed with the port (useful when they are different).
One of the servers is chosen on startup as a preferred one. The module connects to this server with a fail-over policy which can be defined through the action parameter "recover_policy".

The module launch a back-ground thread to monitor the connection. As soon as the connection fails, the thread retries to reestablish the connection and switch to the back-up server if needed to recover the service. While connected to backup server, the thread tries to reconnect to the preferred server using a "recover_policy". This behaviour allow to load balance the client across the two rabbitmq servers on normal conditions, switch to the running server in case of failure and rebalanced on the two server as soon as the failed server is recovered without restarting clients.

The recover policy is based on 4 parameters :

- `check_interval` is the base duration between connection retries (default is 60 seconds)

- `short_failure_interval` is a duration under which two successive failures are considered as abnormal for the rabbitmq server (default is `check_interval/10`)

- `short_failure_nb_max` is the number of successive short failure are detected before to apply the graceful interval (default is 3)

- `graceful_interval` is a longer duration used if the rabbitmq server is unstable (default is `check_interval*10`).

The short failures detection is applied in case of unstable network or server and force to switch to back-up server for at least 'graceful-interval' avoiding heavy load on the unstable server. This can avoid dramatic scenarios in a multisites deployment.

Examples
========

Example 1
---------

This is the simplest action : 

- No High Availability

- The routing-key is constant

- The sent message use JSON format

.. code-block:: none

    module(load='omrabbitmq')
    action(type="omrabbitmq" 
           host="localhost"
           virtual_host="/"
           user="guest"
           password="guest"
           exchange="syslog"
           routing_key="syslog.all")

Example 2
---------

Action characteristics :

- No High Availability

- The routing-key is computed

- The sent message is a raw message

.. code-block:: none

    module(load='omrabbitmq')
    template(name="rkTpl" type="string" string="%syslogtag%.%syslogfacility-text%.%syslogpriority-text%")

    action(type="omrabbitmq" 
           host="localhost"
           virtual_host="/"
           user="guest"
           password="guest"
           exchange="syslog"
           routing_key_template="rkTpl"
           template_body="")

Example 3
---------

HA action : 

- High Availability between `server1:5672` and `server2:1234`

- The routing-key is computed

- The sent message is formatted using RSYSLOG_ForwardFormat standard template

.. code-block:: none

    module(load='omrabbitmq')
    template(name="rkTpl" type="string" string="%syslogtag%.%syslogfacility-text%.%syslogpriority-text%")

    action(type="omrabbitmq" 
           host="server1 server2:1234"
           virtual_host="production"
           user="guest"
           password="guest"
           exchange="syslog"
           routing_key_template="rkTpl"
           template_body="RSYSLOG_ForwardFormat")

Example 4
---------

SSL enabled connection, with Heartbeat : 

- No High Availability

- The routing-key is constant

- The sent message use JSON format

- Heartbeat is set to 20 seconds

.. code-block:: none

    module(load='omrabbitmq')
    action(type="omrabbitmq" 
           host="localhost"
           virtual_host="/"
           user="guest"
           password="guest"
           ssl="on"
           verify_peer="off"
           verify_hostname="off"
           heartbeat_interval="20"
           exchange="syslog"
           routing_key="syslog.all")

******************************
omhiredis: Redis Output Module
******************************

===========================  ===========================================================================
**Module Name:**             **omhiredis**
**Author:**                  Brian Knox <bknox@digitalocean.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for writing to Redis,
using the hiredis client library.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Name or address of the Redis server


ServerPort
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "6379", "no", "none"

Port of the Redis server if the server is not listening on the default port.


ServerPassword
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Password to support authenticated redis database server to push messages
across networks and datacenters. Parameter is optional if not provided
AUTH command wont be sent to the server.


Mode
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "template", "no", "none"

Mode to run the output action in: "queue" or "publish". If not supplied, the
original "template" mode is used.

.. note::

   Due to a config parsing bug in 8.13, explicitly setting this to "template" mode will result in a config parsing error.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_ForwardFormat", "no", "none"

Template is required if using "template" mode.


Key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Key is required if using "publish" or "queue" mode.


Userpush
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If set to on, use RPUSH instead of LPUSH, if not set or off, use LPUSH.


Examples
========

Example 1
---------

*Mode: template*

In "template" mode, the string constructed by the template is sent
to Redis as a command.

.. note::

   This mode has problems with strings with spaces in them - full message won't work correctly. In this mode, the template argument is required, and the key argument is meaningless.

.. code-block:: none

   module(load="omhiredis")

   template(
     name="program_count_tmpl"
     type="string"
     string="HINCRBY progcount %programname% 1")

   action(
     name="count_programs"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="template"
     template="program_count_tmpl")


Example 2
---------

Here's an example redis-cli session where we HGETALL the counts:

.. code-block:: none

   > redis-cli
   127.0.0.1:6379> HGETALL progcount
   1) "rsyslogd"
   2) "35"
   3) "rsyslogd-pstats"
   4) "4302"


Example 3
---------

*Mode: queue*

In "queue" mode, the syslog message is pushed into a Redis list
at "key", using the LPUSH command. If a template is not supplied,
the plugin will default to the RSYSLOG_ForwardFormat template.

.. code-block:: none

   module(load="omhiredis")

   action(
     name="push_redis"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="queue"
     key="my_queue")


Example 3
---------

Here's an example redis-cli session where we RPOP from the queue:

.. code-block:: none

   > redis-cli
   127.0.0.1:6379> RPOP my_queue

   "<46>2015-09-17T10:54:50.080252-04:00 myhost rsyslogd: [origin software=\"rsyslogd\" swVersion=\"8.13.0.master\" x-pid=\"6452\" x-info=\"http://www.rsyslog.com\"] start"

   127.0.0.1:6379>


Example 4
---------

*Mode: publish*

In "publish" mode, the syslog message is published to a Redis
topic set by "key".  If a template is not supplied, the plugin
will default to the RSYSLOG_ForwardFormat template.

.. code-block:: none

   module(load="omhiredis")

   action(
     name="publish_redis"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="publish"
     key="my_channel")


Example 5
---------

Here's an example redis-cli session where we SUBSCRIBE to the topic:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> subscribe my_channel

   Reading messages... (press Ctrl-C to quit)

   1) "subscribe"

   2) "my_channel"

   3) (integer) 1

   1) "message"

   2) "my_channel"

   3) "<46>2015-09-17T10:55:44.486416-04:00 myhost rsyslogd-pstats: {\"name\":\"imuxsock\",\"origin\":\"imuxsock\",\"submitted\":0,\"ratelimit.discarded\":0,\"ratelimit.numratelimiters\":0}"



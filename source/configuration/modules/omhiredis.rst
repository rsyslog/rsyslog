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

Mode to run the output action in: "queue", "publish" or "set". If not supplied, the
original "template" mode is used.

.. note::

   Due to a config parsing bug in 8.13, explicitly setting this to "template" mode will result in a config parsing
   error.

   If mode is "set", omhiredis will send SET commands. If "expiration" parameter is provided (see parameter below),
   omhiredis will send SETEX commands.


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

Key is required if using "publish", "queue" or "set" modes.


Userpush
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If set to on, use RPUSH instead of LPUSH, if not set or off, use LPUSH.


Expiration
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "0", "no", "none"

Only applicable with mode "set". Specifies an expiration for the keys set by omhiredis.
If this parameter is not specified, the value will be 0 so keys will last forever, otherwise they will last for X
seconds.

Examples
========

Example 1: Template mode
------------------------

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


Results
^^^^^^^

Here's an example redis-cli session where we HGETALL the counts:

.. code-block:: none

   > redis-cli
   127.0.0.1:6379> HGETALL progcount
   1) "rsyslogd"
   2) "35"
   3) "rsyslogd-pstats"
   4) "4302"


Example 2: Queue mode
---------------------

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


Results
^^^^^^^

Here's an example redis-cli session where we RPOP from the queue:

.. code-block:: none

   > redis-cli
   127.0.0.1:6379> RPOP my_queue

   "<46>2015-09-17T10:54:50.080252-04:00 myhost rsyslogd: [origin software=\"rsyslogd\" swVersion=\"8.13.0.master\" x-pid=\"6452\" x-info=\"http://www.rsyslog.com\"] start"

   127.0.0.1:6379>


Example 3: Publish mode
-----------------------

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


Results
^^^^^^^

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


Example 4: Set mode
-------------------

In "set" mode, the syslog message is set as a Redis key at "key". If a template is not supplied,
the plugin will default to the RSYSLOG_ForwardFormat template.

.. code-block:: none

   module(load="omhiredis")

   action(
     name="set_redis"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="set"
     key="my_key")


Results
^^^^^^^
Here's an example redis-cli session where we get the key:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> get my_key

   "<46>2019-12-17T20:16:54.781239+00:00 localhost rsyslogd-pstats: { \"name\": \"main Q\", \"origin\": \"core.queue\",
   \"size\": 3, \"enqueued\": 7, \"full\": 0, \"discarded.full\": 0, \"discarded.nf\": 0, \"maxqsize\": 3 }"

   127.0.0.1:6379> ttl my_key

   (integer) -1


Example 5: Set mode with expiration
-----------------------------------

In "set" mode when "expiration" is set to a positive integer, the syslog message is set as a Redis key at "key",
with expiration "expiration".
If a template is not supplied, the plugin will default to the RSYSLOG_ForwardFormat template.

.. code-block:: none

   module(load="omhiredis")

   action(
     name="set_redis"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="set"
     key="my_key"
     expiration="10")


Results
^^^^^^^

Here's an example redis-cli session where we get the key and test the expiration:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> get my_key

   "<46>2019-12-17T20:16:54.781239+00:00 localhost rsyslogd-pstats: { \"name\": \"main Q\", \"origin\": \"core.queue\",
   \"size\": 3, \"enqueued\": 7, \"full\": 0, \"discarded.full\": 0, \"discarded.nf\": 0, \"maxqsize\": 3 }"

   127.0.0.1:6379> ttl my_key

   (integer) 10

   127.0.0.1:6379> ttl my_key

   (integer) 3

   127.0.0.1:6379> ttl my_key

   (integer) -2

   127.0.0.1:6379> get my_key

   (nil)

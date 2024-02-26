******************************
omhiredis: Redis Output Module
******************************

===========================  ===========================================================================
**Module Name:**             **omhiredis**
**Author:**                  Brian Knox <bknox@digitalocean.com>
**Contributors:**            Theo Bertin <theo.bertin@advens.fr>
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

Mode to run the output action in: "queue", "publish", "set" or "stream" If not supplied, the
original "template" mode is used.

.. note::

   Due to a config parsing bug in 8.13, explicitly setting this to "template" mode will result in a config parsing
   error.

   If mode is "set", omhiredis will send SET commands. If "expiration" parameter is provided (see parameter below),
   omhiredis will send SETEX commands.

   If mode is "stream", logs will be sent using XADD. In that case, the template-formatted message will be inserted in
   the **msg** field of the stream ID (this behaviour can be controlled through the :ref:`omhiredis_streamoutfield` parameter)

.. _omhiredis_template:

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_ForwardFormat", "no", "none"

.. Warning::
   Template is required if using "template" mode.


Key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Key is required if using "publish", "queue" or "set" modes.


Dynakey
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If set to "on", the key value will be considered a template by Rsyslog.
Useful when dynamic key generation is desired.

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

.. _omhiredis_streamoutfield:

stream.outField
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "msg", "no", "none"

| Only applicable with mode "stream".
| The stream ID's field to use to insert the generated log.

.. note::
   Currently, the module cannot use the full message object, so it can only insert templated messages to a single stream entry's specific field


.. _omhiredis_streamcapacitylimit:

stream.capacityLimit
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "positive integer", "0", "no", "none"

| Only applicable with mode "stream".
| If set to a value greater than 0 (zero), the XADD will add a `MAXLEN <https://redis.io/docs/data-types/streams-tutorial/#capped-streams>`_ option with **approximate** trimming, limiting the amount of stored entries in the stream at each insertion.

.. Warning::
   This parameter has no way to check if the deleted entries have been ACK'ed once or even used, this should be set if you're sure the insertion rate in lower that the dequeuing to avoid losing entries!


.. _omhiredis_streamack:

stream.ack
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

| Only applicable with mode "stream".
| If set, the module will send an acknowledgement to Redis, for the stream defined by :ref:`omhiredis_streamkeyack`, with the group defined by :ref:`omhiredis_streamgroupack` and the ID defined by :ref:`omhiredis_streamindexack`.
| This is especially useful when used with the :ref:`imhiredis_stream_consumerack` deactivated, as it allows omhiredis to acknowledge the correct processing of the log once the job is effectively done.


.. _omhiredis_streamdel:

stream.del
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

| Only applicable with mode "stream".
| If set, the module will send a XDEL command to remove an entry, for the stream defined by :ref:`omhiredis_streamkeyack`, and the ID defined by :ref:`omhiredis_streamindexack`.
| This can be useful to automatically remove processed entries extracted on a previous stream by imhiredis.


.. _omhiredis_streamkeyack:

stream.keyAck
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

| Only applicable with mode "stream".
| Is **required**, if one of :ref:`omhiredis_streamack` or :ref:`omhiredis_streamdel` are **on**.
| Defines the value to use for acknowledging/deleting while inserting a new entry, can be either a constant value or a template name if :ref:`omhiredis_streamdynakeyack` is set.
| This can be useful to automatically acknowledge/remove processed entries extracted on a previous stream by imhiredis.


.. _omhiredis_streamdynakeyack:

stream.dynaKeyAck
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

| Only applicable with mode "stream".
| If set to **on**, the value of :ref:`omhiredis_streamkeyack` is taken as an existing Rsyslog template.



.. _omhiredis_streamgroupack:

stream.groupAck
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

| Only applicable with mode "stream".
| Is **required**, if :ref:`omhiredis_streamack` is **on**.
| Defines the value to use for acknowledging while inserting a new entry, can be either a constant value or a template name if :ref:`omhiredis_streamdynagroupack` is set.
| This can be useful to automatically acknowledge processed entries extracted on a previous stream by imhiredis.


.. _omhiredis_streamdynagroupack:

stream.dynaGroupAck
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

| Only applicable with mode "stream".
| If set to **on**, the value of :ref:`omhiredis_streamgroupack` is taken as an existing Rsyslog template.


.. _omhiredis_streamindexack:

stream.indexAck
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

| Only applicable with mode "stream".
| Is **required**, if one of :ref:`omhiredis_streamack` or :ref:`omhiredis_streamdel` are **on**.
| Defines the index value to use for acknowledging/deleting while inserting a new entry, can be either a constant value or a template name if :ref:`omhiredis_streamdynaindexack` is set.
| This can be useful to automatically acknowledge/remove processed entries extracted on a previous stream by imhiredis.


.. _omhiredis_streamdynaindexack:

stream.dynaIndexAck
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

| Only applicable with mode "stream".
| If set to **on**, the value of :ref:`omhiredis_streamindexack` is taken as an existing Rsyslog template.

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


Example 6: Set mode with dynamic key
------------------------------------

In any mode with "key" defined and "dynakey" as "on", the key used during operation will be dynamically generated
by Rsyslog using templating.

.. code-block:: none

   module(load="omhiredis")

   template(name="example-template" type="string" string="%hostname%")

   action(
     name="set_redis"
     server="my-redis-server.example.com"
     serverport="6379"
     type="omhiredis"
     mode="set"
     key="example-template"
     dynakey="on")


Results
^^^^^^^
Here's an example redis-cli session where we get the dynamic key:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> keys *

   (empty list or set)

   127.0.0.1:6379> keys *

   1) "localhost"


Example 7: "Simple" stream mode
-------------------------------

| By using the **stream mode**, the template-formatted log is inserted in a stream using the :ref:`omhiredis_streamoutfield` parameter as key (or *msg* as default).
| The output template can be explicitely set with the :ref:`omhiredis_template` option (or the default *RSYSLOG_ForwardFormat* template will be used).

.. code-block:: none

   module(load="omhiredis")

   template(name="example-template" type="string" string="%hostname%")

   action(
     type="omhiredis"
     server="my-redis-server.example.com"
     serverport="6379"
     template="example-template"
     mode="stream"
     key="stream_output"
     stream.outField="data")


Results
^^^^^^^
Here's an example redis-cli session where we get the newly inserted stream index:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> XLEN stream_output
   1

   127.0.0.1:6379> xread STREAMS stream_output 0
   1) 1) "stream_output"
      2) 1) 1) "1684507855284-0"
            2) 1) "data"
               2) "localhost"


Example 8: Get from a stream with imhiredis, then insert in another one with omhiredis
--------------------------------------------------------------------------------------

| When you use the omhiredis in stream mode with the imhiredis in stream mode as input, you might want to acknowledge
 entries taken with imhiredis once you insert them back somewhere else with omhiredis.
| The module allows to acknowledge input entries using information either provided by the user through configuration
 or through information accessible in the log entry.
| Under the hood, imhiredis adds metadata to the generated logs read from redis streams, this data includes
 the stream name, ID of the index, group name and consumer name (when read from a consumer group).
| This information is added in the **$.redis** object and can be retrieved with the help of specific templates.

.. code-block:: none

   module(load="imhiredis")
   module(load="omhiredis")

   template(name="redisJsonMessage" type="list") {
         property(name="$!output")
   }

   template(name="indexTemplate" type="list") {
         property(name="$.redis!index")
   }
   # Not used in this example, but can be used to replace the static declarations in omhiredis' configuration below
   template(name="groupTemplate" type="list") {
         property(name="$.redis!group")
   }
   template(name="keyTemplate" type="list") {
         property(name="$.redis!stream")
   }

   input(type="imhiredis"
           server="127.0.0.1"
           port="6379"
           mode="stream"
           key="stream_input"
           stream.consumerGroup="group1"
           stream.consumerName="consumer1"
           stream.consumerACK="off"
           ruleset="receive_redis")

   ruleset(name="receive_redis") {

      action(type="omhiredis"
               server="127.0.0.1"
               serverport="6379"
               mode="stream"
               key="stream_output"
               stream.ack="on"
               # The key and group values are set statically, but the index value is taken from imhiredis metadata
               stream.dynaKeyAck="off"
               stream.keyAck="stream_input"
               stream.dynaGroupAck="off"
               stream.groupAck="group1"
               stream.indexAck="indexTemplate"
               stream.dynaIndexAck="on"
               template="redisJsonMessage"
            )
   }


Results
^^^^^^^
Here's an example redis-cli session where we get the pending entries at the end of the log re-insertion:

.. code-block:: none

   > redis-cli

   127.0.0.1:6379> XINFO GROUPS stream_input
   1)  1) "name"
      1) "group1"
      2) "consumers"
      3) (integer) 1
      4) "pending"
      5) (integer) 0
      6) "last-delivered-id"
      7) "1684509391900-0"
      8) "entries-read"
      9)  (integer) 1
      10) "lag"
      11) (integer) 0



Example 9: Ensuring streams don't grow indefinitely
---------------------------------------------------

| While using Redis streams, index entries are not automatically evicted, even if you acknowledge entries.
| You have several options to ensure your streams stays under reasonable memoyr usage, while making sure your data is
 not evicted before behing processed.
| To do that, you have 2 available options, that can be used independently from each other
 (as they don't apply to the same source):

   - **stream.del** to delete processed entries (can also be used as a complement of ACK'ing)
   - **stream.capacityLimit** that allows to ensure a hard-limit of logs can be inserted before dropping entries

.. code-block:: none

   module(load="imhiredis")
   module(load="omhiredis")

   template(name="redisJsonMessage" type="list") {
         property(name="$!output")
   }

   template(name="indexTemplate" type="list") {
         property(name="$.redis!index")
   }
   template(name="keyTemplate" type="list") {
         property(name="$.redis!stream")
   }

   input(type="imhiredis"
           server="127.0.0.1"
           port="6379"
           mode="stream"
           key="stream_input"
           ruleset="receive_redis")

   ruleset(name="receive_redis") {

      action(type="omhiredis"
               server="127.0.0.1"
               serverport="6379"
               mode="stream"
               key="stream_output"
               stream.capacityLimit="1000000"
               stream.del="on"
               stream.dynaKeyAck="on"
               stream.keyAck="keyTemplate"
               stream.dynaIndexAck="on"
               stream.indexAck="indexTemplate"
               template="redisJsonMessage"
            )
   }


Results
^^^^^^^
Here, the result of this configuration is:

   - entries are deleted from the source stream *stream_input* after being inserted by omhiredis to *stream_output*
   - *stream_output* won't hold more than (approximately) a million entries at a time

.. Warning::
   The **stream.capacityLimit** is an approximate maximum! see `redis documentation on MAXLEN and the '~' option <https://redis.io/commands/xadd>`_ to understand how it works!

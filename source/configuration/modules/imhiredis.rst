
.. include:: <isonum.txt>

*****************************
Imhiredis: Redis input plugin
*****************************

====================  =====================================
**Module Name:**      **imhiredis**
**Author:**           Jeremie Jourdin <jeremie.jourdin@advens.fr>
**Contributors:**     Theo Bertin <theo.bertin@advens.fr>
====================  =====================================

Purpose
=======

Imhiredis is an input module reading arbitrary entries from Redis.
It uses the `hiredis library <https://github.com/redis/hiredis.git>`_ to query Redis instances using 3 modes:

- **queues**, using `LIST <https://redis.io/commands#list>`_ commands
- **channels**, using `SUBSCRIBE <https://redis.io/commands#pubsub>`_ commands
- **streams**, using `XREAD/XREADGROUP <https://redis.io/commands/?group=stream>`_ commands


.. _imhiredis_queue_mode:

Queue mode
----------

The **queue mode** uses Redis LISTs to push/pop messages to/from lists. It allows simple and efficient uses of Redis as a queueing system, providing both LIFO and FIFO methods.

This mode should be preferred if the user wants to use Redis as a caching system, with one (or many) Rsyslog instances POP'ing out entries.

.. Warning::
	This mode was configured to provide optimal performances while not straining Redis, but as imhiredis has to poll the instance some trade-offs had to be made:

	- imhiredis POPs entries by batches of 10 to improve performances (size of batch is configurable via the batchsize parameter)
	- when no entries are left in the list, the module sleeps for 1 second before checking the list again. This means messages might be delayed by as much as 1 second between a push to the list and a pop by imhiredis (entries will still be POP'ed out as fast as possible while the list is not empty)


.. _imhiredis_channel_mode:

Channel mode
------------

The **subscribe** mode uses Redis PUB/SUB system to listen to messages published to Redis' channels. It allows performant use of Redis as a message broker.

This mode should be preferred to use Redis as a message broker, with zero, one or many subscribers listening to new messages.

.. Warning::
   This mode shouldn't be used if messages are to be reliably processed, as messages published when no Imhiredis is listening will result in the loss of the message.


.. _imhiredis_stream_mode:

Stream mode
------------

The **stream** mode uses `Redis Streams system <https://redis.io/docs/data-types/streams/>`_ to read entries published to Redis' streams. It is a good alternative when:
  - sharing work is desired
  - not losing any log (even in the case of a crash) is mandatory

This mode is especially useful to define pools of workers that do various processing along the way, while ensuring not a single log is lost during processing by a worker.

.. note::
    As Redis streams do not insert simple values in keys, but rather fleid/value pairs, this mode can also be useful when handling structured data. This is better shown with the examples for the parameter :ref:`imhiredis_fields`.

   This mode also adds additional internal metadata to the message, it won't be included in json data or regular fields, but

      - **$.redis!stream** will be added to the message, with the value of the source stream
      - **$.redis!index** will be added to the message, with the exact ID of the entry
      - **$.redis!group** will be added in the message (if :ref:`imhiredis_stream_consumergroup` is set), with the value of the group used to read the entry
      - **$.redis!consumer** will be added in the message (if :ref:`imhiredis_stream_consumername` is set), with the value of the consumer name used to read the entry

   This is especially useful when used with the omhiredis module, to allow it to get the required information semi-automatically (custom templates will still be required in the user configuration)

.. Warning::
   This mode is the most reliable to handle entries stored in Redis, but it might also be the one with the most overhead. Although still minimal, make sure to test the different options and determine if this mode is right for you!


Master/Replica
--------------

This module is able to automatically connect to the master instance of a master/replica(s) cluster. Simply providing a valid connection entry point (being the current master or a valid replica), Imhiredis is able to redirect to the master node on startup and when states change between nodes.


Configuration Parameters
========================

.. note::
    Parameter names are case-insensitive


Input Parameters
----------------

.. _imhiredis_mode:

mode
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "subscribe", "yes", "none"

| Defines the mode to use for the module.
| Should be either "**subscribe**" (:ref:`imhiredis_channel_mode`), "**queue**" (:ref:`imhiredis_queue_mode`) or "**stream**" (:ref:`imhiredis_stream_mode`) (case-sensitive).


ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Assign messages from this input to a specific Rsyslog ruleset.


batchsize
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "10", "yes", "none"

Defines the dequeue batch size for redis pipelining.
imhiredis will read "**batchsize**" elements from redis at a time.

When using the :ref:`imhiredis_queue_mode`, defines the size of the batch to use with LPOP / RPOP.


.. _imhiredis_key:

key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Defines either the name of the list to use (for :ref:`imhiredis_queue_mode`) or the channel to listen to (for :ref:`imhiredis_channel_mode`).


.. _imhiredis_socketPath:

socketPath
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "no", "if no :ref:`imhiredis_server` provided", "none"

Defines the socket to use when trying to connect to Redis. Will be ignored if both :ref:`imhiredis_server` and :ref:`imhiredis_socketPath` are given.


.. _imhiredis_server:

server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "ip", "127.0.0.1", "if no :ref:`imhiredis_socketPath` provided", "none"

The Redis server's IP to connect to.


.. _imhiredis_port:

port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "6379", "no", "none"

The Redis server's port to use when connecting via IP.


.. _imhiredis_password:

password
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The password to use when connecting to a Redis node, if necessary.


.. _imhiredis_uselpop:

uselpop
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "no", "no", "none"

| When using the :ref:`imhiredis_queue_mode`, defines if imhiredis should use a LPOP instruction instead of a RPOP (the default).
| Has no influence on the :ref:`imhiredis_channel_mode` and will be ignored if set with this mode.


.. _imhiredis_stream_consumergroup:

stream.consumerGroup
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

| When using the :ref:`imhiredis_stream_mode`, defines a consumer group name to use (see `the XREADGROUP documentation <https://redis.io/commands/xreadgroup/>`_ for details). This parameter activates the use of **XREADGROUP** commands, in replacement to simple XREADs.
| Has no influence in the other modes (queue or channel) and will be ignored.

.. note::
    If this parameter is set, :ref:`imhiredis_stream_consumername` should also be set


.. _imhiredis_stream_consumername:

stream.consumerName
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

| When using the :ref:`imhiredis_stream_mode`, defines a consumer name to use (see `the XREADGROUP documentation <https://redis.io/commands/xreadgroup/>`_ for details). This parameter activates the use of **XREADGROUP** commands, in replacement to simple XREADs.
| Has no influence in the other modes (queue or channel) and will be ignored.

.. note::
    If this parameter is set, :ref:`imhiredis_stream_consumergroup` should also be set


.. _imhiredis_stream_readfrom:

stream.readFrom
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "$", "no", "none"

| When using the :ref:`imhiredis_stream_mode`, defines the `starting ID <https://redis.io/docs/data-types/streams-tutorial/#entry-ids>`_ for XREAD/XREADGROUP commands (can also use special IDs, see `documentation <https://redis.io/docs/data-types/streams-tutorial/#special-ids-in-the-streams-api>`_).
| Has no influence in the other modes (queue or channel) and will be ignored.


.. _imhiredis_stream_consumerack:

stream.consumerACK
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "on", "no", "none"

| When using :ref:`imhiredis_stream_mode` with :ref:`imhiredis_stream_consumergroup` and :ref:`imhiredis_stream_consumername`, determines if the module should directly acknowledge the ID once read from the Consumer Group.
| Has no influence in the other modes (queue or channel) and will be ignored.

.. note::
    When using Consumer Groups and imhiredis, omhiredis can also integrate with this workflow to acknowledge a processed message once put back in another stream (or somewhere else). This parameter is then useful set to **off** to let the omhiredis module acknowledge the input ID once the message is correctly sent.


.. _imhiredis_stream_autoclaimidletime:

stream.autoclaimIdleTime
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "positive number", "0", "no", "none"

| When using :ref:`imhiredis_stream_mode` with :ref:`imhiredis_stream_consumergroup` and :ref:`imhiredis_stream_consumername`, determines if the module should check for pending IDs that exceed this time (**in milliseconds**) to assume the original consumer failed to acknowledge the log and claim them for their own (see `the redis ducumentation <https://redis.io/docs/data-types/streams-tutorial/#automatic-claiming>`_ on this subject for more details on how that works).
| Has no influence in the other modes (queue or channel) and will be ignored.

.. note::
    If this parameter is set, the AUTOCLAIM operation will also take into account the specified :ref:`imhiredis_stream_readfrom` parameter. **If its value is '$' (default), the AUTOCLAIM commands will use '0-0' as the starting ID**.



.. _imhiredis_fields:

fields
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "[]", "no", "none"

| When using :ref:`imhiredis_stream_mode`, the module won't get a simple entry but will instead get hashes, with field/value pairs.
| By default, the module will insert every value into their respective field in the **$!** object, but this parameter can change this behaviour, for each entry the value will be a string where:

    - if the entry begins with a **!** or a **.**, it will be taken as a key to take into the original entry
    - if the entry doesn't begin with a **!** or a **.**, the value will be taken verbatim
    - in addition, if the value is prefixed with a **:<key>:** pattern, the value (verbatim or taken from the entry) will be inserted in this specific key (or subkey)

*Examples*:

.. csv-table::
   :header: "configuration", "result"
   :widths: auto
   :class: parameter-table

   ``["static_value"]``, the value "static_value" will be inserted in $!static_value
   ``[":key:static_value"]``, the value "static_value" will be inserted in $!key
   ``["!field"]``, the value of the field "field" will be inserted in $!field
   ``[":key!subkey:!field"]``, the value of the field "field" will be inserted in $!key!subkey


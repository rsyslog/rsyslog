
*****************************
Imhiredis: Redis input plugin
*****************************

====================  =====================================
**Module Name:**      **imhiredis**
**Author:**           Jeremie Jourdin <jeremie.jourdin@advens.fr>
====================  =====================================

Purpose
=======

Imhiredis is an input module reading arbitrary entries from Redis.
It uses the `hiredis library <https://github.com/redis/hiredis.git>`_ to query Redis instances using 2 modes:

- **queues**, using `LIST <https://redis.io/commands#list>`_ commands
- **channels**, using `SUBSCRIBE <https://redis.io/commands#pubsub>`_ commands


.. _imhiredis_queue_mode:

Queue mode
----------

The **queue mode** uses Redis LISTs to push/pop messages to/from lists. It allows simple and efficient uses of Redis as a queueing system, providing both LIFO and FIFO methods.

This mode should be preferred if the user wants to use Redis as a caching system, with one (or many) Rsyslog instances POP'ing out entries.

.. Warning::
	This mode was configured to provide optimal performances while not straining Redis, but as imhiredis has to poll the instance some trade-offs had to be made:

	- imhiredis  POPs entries by batches of 10 to improve performances (this is currently not configurable)
	- when no entries are left in the list, the module sleeps for 1 second before checking the list again. This means messages might be delayed by as much as 1 second between a push to the list and a pop by imhiredis (entries will still be POP'ed out as fast as possible while the list is not empty)


.. _imhiredis_channel_mode:

Channel mode
------------

The **subscribe** mode uses Redis PUB/SUB system to listen to messages published to Redis' channels. It allows performant use of Redis as a message broker.

This mode should be preferred to use Redis as a message broker, with zero, one or many subscribers listening to new messages.

.. Warning::
   This mode shouldn't be used if messages are to be reliably processed, as messages published when no Imhiredis is listening will result in the loss of the message.


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

Defines the mode to use for the module.
Should be either "**subscribe**" (:ref:`imhiredis_channel_mode`), or "**queue**" (:ref:`imhiredis_queue_mode`) (case-sensitive).


.. _imhiredis_batchsize:

batchsize
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "10", "yes", "none"

Defines the dequeue batch size for redis pipelining.
imhiredis will read "**batchsize**" elements from redis at a time.


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

When using the :ref:`imhiredis_queue_mode`, defines if imhiredis should use a LPOP instruction instead of a RPOP (the default).
Has no influence on the :ref:`imhiredis_channel_mode` and will be ignored if set with this mode.


ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

   Assign messages from this input to a specific Rsyslog ruleset.

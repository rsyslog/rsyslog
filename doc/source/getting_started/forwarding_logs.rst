Forwarding Logs
===============

rsyslog can forward log messages to remote servers. This is often done
to centralize logs, improve analysis, or send data to SIEM or monitoring
systems.

Minimal Forwarding Example (TCP)
--------------------------------

Add the following snippet to your `/etc/rsyslog.conf` or to a file inside
`/etc/rsyslog.d/`:

.. code-block:: rsyslog

   # Forward all messages to a remote server using TCP.
   # The linked-list queue prevents blocking if the server is temporarily unreachable.
   action(
       type="omfwd"              # Output module for forwarding messages
       protocol="tcp"            # Use TCP (reliable transport)
       target="logs.example.com" # Destination server (replace with your host)
       port="514"                # TCP port on the remote syslog server
       queue.type="linkedList"   # Best practice for network forwarding
   )

Why use `queue.type="linkedList"`?
----------------------------------

When a remote server goes offline, a direct TCP forwarding action can
block and delay local logging. Using a queue ensures that messages are
stored temporarily and sent once the connection recovers. This is a
recommended default for TCP-based forwarding.

Forwarding via UDP
------------------

UDP is a connectionless protocol and does not block, so queues are not
required in this case. To forward messages via UDP, modify the protocol:

.. code-block:: rsyslog

   # Forward all messages to a remote server using UDP.
   action(
       type="omfwd"
       protocol="udp"            # UDP (unreliable, but lower overhead)
       target="logs.example.com"
       port="514"
   )

Testing the Connection
----------------------

To verify that logs are reaching the remote server:

1. Send a test message locally:
   .. code-block:: bash

      logger "test message from $(hostname)"

2. Check the remote server's logs for the test message.

Advanced Queue Tuning
---------------------

The default queue parameters work for most cases. For high performance
or large bursts of logs, you can adjust settings such as:

- `queue.size` – Number of messages stored in the queue.
- `queue.dequeueBatchSize` – Number of messages processed per batch.

See :doc:`../rainerscript/queue_parameters` for details.


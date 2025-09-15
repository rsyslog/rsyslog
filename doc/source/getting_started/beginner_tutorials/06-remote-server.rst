.. _tut-06-remote-server:

Your First Remote Log Server
############################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog remote server, imudp, log receiver, central logging

.. summary-start

Set up rsyslog to **receive logs from another machine** over UDP.
Use a **dedicated ruleset** so only remote messages go into ``/var/log/remote.log``.

.. summary-end

Goal
====

Create a basic **remote log receiver**.
You will configure rsyslog to listen on UDP/514 and process incoming messages
with a separate ruleset, ensuring local logs remain unaffected.

.. important::

   This tutorial requires **two systems** (or two containers/VMs).
   One acts as the **server** (receiver), the other as the **client** (sender).
   Without a second machine, forwarding may appear “stuck” because rsyslog retries.

Steps
=====

1) Configure the server (receiver)
----------------------------------

On the receiving system, create ``/etc/rsyslog.d/10-receiver.conf``:

.. code-block:: rsyslog

   # Load UDP input
   module(load="imudp")

   # A ruleset just for messages received via this UDP listener
   ruleset(name="rs-from-udp") {
       action(type="omfile" file="/var/log/remote.log")
       # This ruleset is used only for the UDP input below.
       # Local system logs continue to use the default distro config.
   }

   # Assign the UDP input to the ruleset above
   input(type="imudp" port="514" ruleset="rs-from-udp")

Restart rsyslog:

.. code-block:: bash

   sudo systemctl restart rsyslog
   systemctl status rsyslog --no-pager

2) Configure the client (sender)
--------------------------------

On the sending system, create ``/etc/rsyslog.d/10-forward.conf``:

.. code-block:: rsyslog

   # Forward all messages via UDP to the server
   action(
       type="omfwd"
       target="server.example.com"   # replace with server hostname or IP
       port="514"
       protocol="udp"
   )

Restart rsyslog on the client:

.. code-block:: bash

   sudo systemctl restart rsyslog

3) Test the setup
-----------------

From the **client**, send a test message:

.. code-block:: bash

   logger -t tut06 "hello from the client"

On the **server**, check the remote log file:

.. code-block:: bash

   sudo tail -n 20 /var/log/remote.log

You should see the test message.
Only messages from the client appear here, because the UDP input uses its own ruleset.

If it’s not working…
=====================

1. **No messages arrive**

   - Verify the server is listening on UDP/514:

     .. code-block:: bash

        sudo ss -ulpn | grep ':514'

   - Check firewall rules (``ufw`` or ``firewalld``) to allow UDP/514.
   - Ensure the client’s ``target=`` hostname/IP is correct (try an IP to rule out DNS).

2. **Messages appear only on the client**

   - Test network reachability:

     .. code-block:: bash

        ping server.example.com

   - If ICMP/ping is blocked, check with traceroute or review firewall/NAT.

3. **Permission denied on /var/log/remote.log**

   - Ensure rsyslog has permission to write under ``/var/log/``.
   - For testing, root-owned files in ``/var/log/`` are fine.

4. **Service won’t start**

   - Validate configuration on both systems:

     .. code-block:: bash

        sudo rsyslogd -N1

Verification checkpoint
=======================

By the end of this tutorial you should be able to:

- Restart rsyslog cleanly on both client and server.
- Send a message with ``logger`` on the client.
- See the message arrive in ``/var/log/remote.log`` on the server, without local logs mixed in.

See also / Next steps
=====================

- :doc:`04-message-pipeline` – how inputs, rulesets, and actions fit together.
- :doc:`../forwarding_logs` – more on forwarding (UDP vs TCP) and queues.
- Reference: :doc:`../../configuration/modules/imudp`
- Reference: :doc:`../../configuration/modules/omfwd`

----

.. note::

   Forwarding requires a **reachable** server. Without a valid target (and without
   an action queue), rsyslog may retry and appear “stuck” for a while.

.. tip::

   🎬 *Video idea (3–4 min):* show two terminals (client/server), run ``logger``
   on the client, and tail ``/var/log/remote.log`` on the server. Then point
   out the dedicated ruleset in the config that keeps local logs separate.

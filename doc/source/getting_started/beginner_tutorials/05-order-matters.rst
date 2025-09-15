.. _tut-05-order-matters:

Order Matters: Config and Include Files
#######################################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog order, config sequence, rsyslog.d, stop, discard

.. summary-start

Learn how rsyslog processes configuration **in order**, why file ordering in
``/etc/rsyslog.d/`` matters, and how earlier rules affect later ones.

.. summary-end

Goal
====

Understand that rsyslog executes rules **sequentially**.
The order of actions and included files can change results.

Key principle
=============

- Rules in the same file run **top to bottom**.
- Files in ``/etc/rsyslog.d/`` are processed in **lexical order** (e.g.,
  ``10-first.conf`` runs before ``50-extra.conf``).
- An earlier rule can **discard or modify messages**, so later rules may never see them.

Hands-on example
================

1) Create ``/etc/rsyslog.d/10-drop.conf``:

.. code-block:: rsyslog

   if ($programname == "tut05") then {
       stop    # discard these messages, no further actions
   }

2) Create ``/etc/rsyslog.d/20-log.conf``:

.. code-block:: rsyslog

   if ($programname == "tut05") then {
       action(type="omfile" file="/var/log/tut05.log")
   }

3) Restart rsyslog:

.. code-block:: bash

   sudo systemctl restart rsyslog

4) Send a test message:

.. code-block:: bash

   logger -t tut05 "hello from tutorial 05"

Expected result
---------------

No file ``/var/log/tut05.log`` is created.
The first snippet (`10-drop.conf`) discards the message before the logging rule runs.

Switch the order
================

Rename the files to reverse order:

.. code-block:: bash

   sudo mv /etc/rsyslog.d/10-drop.conf /etc/rsyslog.d/50-drop.conf
   sudo systemctl restart rsyslog
   logger -t tut05 "hello after reorder"

Now ``/var/log/tut05.log`` will contain the message, because the logging rule ran first.

If it’s not working…
=====================

1. **Still no log file**

   - Check snippet order with:
     ``ls -1 /etc/rsyslog.d/``
   - Ensure ``20-log.conf`` comes **before** ``50-drop.conf``.

2. **File exists but is empty**

   - Confirm you used the correct tag:
     ``logger -t tut05 "…"``

3. **Syntax errors**

   - Validate your config:
     ``sudo rsyslogd -N1``

Verification checkpoint
=======================

By the end of this tutorial you should be able to:

- Explain that rsyslog rules run **top to bottom, file by file**.
- Use file naming (``10-…``, ``50-…``) to control execution order.
- Predict why a later action might never see a message.

See also / Next steps
=====================

- :doc:`04-message-pipeline` – how messages flow through inputs, rulesets, and actions.
- :doc:`../basic_configuration` – reference example of a simple config.
- :doc:`../forwarding_logs` – adding network forwarding.

----

.. tip::

   🎬 *Video idea (3 min):* show two snippet files, run ``logger -t tut05 …``,
   then swap the file order and rerun. Visualize how rsyslog processes files
   in lexical order.

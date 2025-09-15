.. _tut-02-first-config:

Your First Configuration
########################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog first config, logger, omfile, rsyslog.d

.. summary-start

Write a minimal RainerScript configuration that logs a specific test message to its own file, test it with ``logger``,
and verify with ``tail -f`` — without changing distro-provided inputs.

.. summary-end

Goal
====

Create your first custom rsyslog configuration in modern **RainerScript** syntax.
You will add a tiny rule that writes **only your test message** into a new file,
so you don’t duplicate all system logs.

.. important::
   Most distributions already configure inputs (on Ubuntu this is often ``imjournal``,
   sometimes ``imuxsock``). **Do not load input modules here.** We’ll just add a safe,
   small rule in ``/etc/rsyslog.d/``. For background, see :doc:`03-default-config`.

Steps
=====

1) Create a new config snippet
------------------------------

Create ``/etc/rsyslog.d/10-first.conf`` with this content:

.. code-block:: rsyslog

   # Write only messages tagged "tut02" to a custom file
   if ($programname == "tut02") then {
       action(type="omfile" file="/var/log/myfirst.log")
       # no 'stop' here: allow normal distro handling to continue
   }

Why this approach?
------------------

- We **don’t** touch inputs (distro already set them up).
- We **filter by tag** so only your test message goes to the new file, keeping it clean.
- We **don’t** use ``stop`` so normal logging continues unchanged.

2) Restart rsyslog
------------------

.. code-block:: bash

   sudo systemctl restart rsyslog
   systemctl status rsyslog --no-pager

3) Send a test message
----------------------

Use the ``logger`` command to generate a message with the tag ``tut02``:

.. code-block:: bash

   logger -t tut02 "hello from rsyslog tutorial 02"

4) Verify the result
--------------------

Check the new file:

.. code-block:: bash

   sudo tail -f /var/log/myfirst.log

You should see your message. The system’s regular logs (e.g., ``/var/log/syslog`` on Ubuntu
or ``/var/log/messages`` on RHEL-like distros) continue to work as before.

If it’s not working…
=====================

1. **No file created**

   - Service status: ``systemctl status rsyslog``
   - Syntax check: ``sudo rsyslogd -N1``
   - Ensure the snippet path is correct: ``/etc/rsyslog.d/10-first.conf``

2. **File exists but no message inside**

   - Confirm you used the **exact tag**: ``logger -t tut02 "..."``
   - Verify the filter matches: it checks ``$programname == "tut02"``

3. **Permission denied**

   - Ensure rsyslog can write to ``/var/log/`` (default root-owned is fine). For custom paths,
     adjust ownership/permissions (``sudo chown`` / ``chmod``) as needed.

4. **Ubuntu-specific note**

   - Ubuntu typically uses ``imjournal`` by default. That’s fine — this rule still works.
     If you previously tried to load inputs manually, remove those lines and restart.

Verification checkpoint
=======================

By the end of this tutorial you should be able to:

- Restart rsyslog without syntax errors.
- Send a tagged test message with ``logger``.
- See the message in your custom file without duplicating all system logs.

See also / Next steps
=====================

- :doc:`03-default-config` – why your distribution’s default config uses different syntax,
  and how to add modern snippets safely alongside it.
- :doc:`04-message-pipeline` – understand the flow: input → ruleset → action.
- Existing page: :doc:`../basic_configuration` – neutral reference example.

----

.. tip::
   🎬 *Video idea (3 min):* create ``10-first.conf``, restart rsyslog, run
   ``logger -t tut02 "…"`` and watch ``/var/log/myfirst.log`` update live with ``tail -f``.

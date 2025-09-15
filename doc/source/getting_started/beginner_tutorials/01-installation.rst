.. _tut-01-installation:

Installing rsyslog
##################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog install, rsyslog service, rsyslogd -N1, docker

.. summary-start

Install rsyslog via packages, verify the service, and (optionally) try a Docker sandbox.

.. summary-end

Goal
====

Get rsyslog installed and confirm it runs correctly on your system.
If you prefer a zero-risk sandbox, try the optional Docker approach at the end.

.. important::
   **About default distro configs:** Many distributions ship legacy-style config
   lines in ``/etc/rsyslog.conf`` (e.g., ``*.* /var/log/syslog`` or ``$FileCreateMode``).
   That is **normal** and supported. In these tutorials we use **modern RainerScript**.
   **Do not rewrite the distro file.** Add your own rules under ``/etc/rsyslog.d/*.conf``.
   For a guided explanation, see :doc:`03-default-config`.

Steps
=====

1) Install the packages
-----------------------

On **Ubuntu/Debian**:

.. code-block:: bash

   sudo apt update
   sudo apt install rsyslog

On **RHEL / CentOS / Rocky / Alma**:

.. code-block:: bash

   sudo dnf install rsyslog

2) Enable and start the service
-------------------------------

.. code-block:: bash

   sudo systemctl enable --now rsyslog
   systemctl status rsyslog --no-pager

3) Validate configuration syntax
--------------------------------

Run a dry-run parse to check syntax without launching a second daemon:

.. code-block:: bash

   sudo rsyslogd -N1

You should see **“rsyslogd: End of config validation run.”** with no errors.

Verification
============

Send a test message and ensure rsyslog is processing logs locally:

.. code-block:: bash

   logger -t tut01 "hello from rsyslog tutorial 01"
   sudo tail -n 50 /var/log/syslog  2>/dev/null || sudo tail -n 50 /var/log/messages

You should see a line containing ``tut01`` and your message.

If it’s not working…
=====================

1. **Service not active**

   - Check: ``systemctl status rsyslog``
   - Fix: ``sudo systemctl restart rsyslog``

2. **Syntax errors**

   - Run: ``sudo rsyslogd -N1``
   - Read the first error carefully; it points to the file/line. Remove the offending
     change or fix the typo, then re-run.

3. **Logs not visible**

   - Different distros write to different files. Try both:
     ``/var/log/syslog`` and ``/var/log/messages``.
   - Ensure your terminal command used ``logger`` (see above).

4. **Permission issues**

   - If you created custom log paths, ensure directory write permissions for the
     rsyslog service user. Use ``sudo chown`` / ``chmod`` appropriately.

Optional: Try rsyslog in Docker (sandbox)
=========================================

Use this if you want to **experiment without touching your host’s system logger**.

.. code-block:: bash

   docker run --name rsyslog-sandbox -it --rm rsyslog/rsyslog

In another terminal, exec a shell into the container to test:

.. code-block:: bash

   docker exec -it rsyslog-sandbox bash
   logger -t tut01 "hello from inside container"
   tail -n 50 /var/log/syslog  2>/dev/null || tail -n 50 /var/log/messages

.. note::
   This container **does not replace** your host’s system logger. To receive host
   logs, you’d need volume mounts and socket plumbing; that is outside this beginner
   tutorial and covered later in best-practice guidance.

See also / Next steps
=====================

- :doc:`02-first-config` – write a message to a custom file using modern RainerScript.
- :doc:`03-default-config` – why distro configs look “old”, and how to add your own rules safely.
- Existing page: :doc:`../installation` – neutral installation reference.

----

.. tip::
   🎬 *Video idea:* a 2–3 min screen capture showing package install, service check,
   ``rsyslogd -N1``, a ``logger`` test, and the Docker sandbox run.

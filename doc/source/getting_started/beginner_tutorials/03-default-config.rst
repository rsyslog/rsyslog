.. _tut-03-default-config:

Understanding the Default Configuration
#######################################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog default config, imjournal, imuxsock, legacy syntax

.. summary-start

Why your distro’s default rsyslog config looks “old”, what those lines mean,
and how to safely add modern snippets alongside it.

.. summary-end

Goal
====

Understand why the configuration you see in ``/etc/rsyslog.conf`` may look different
from these tutorials, and learn the safe way to extend it without breaking
your distribution’s setup.

Why it looks different
======================

When you open ``/etc/rsyslog.conf`` on a freshly installed system, you might see
directives like:

.. code-block:: none

   *.* /var/log/syslog
   $FileCreateMode 0640

These come from how Linux distributions ship rsyslog. It is a **compatibility choice**
to preserve behavior from older syslog systems. At the same time, the distro config
often loads modern modules such as:

.. code-block:: rsyslog

   module(load="imuxsock")
   module(load="imjournal")

This mix of legacy and modern syntax can look confusing.
The key point: **both styles work.** For new configs, always use **RainerScript**.

Want to know what a legacy line like ``$FileCreateMode`` actually does?
You don’t need to learn all of these right now, but if you’re curious,
try the `AI rsyslog assistant <https://rsyslog.ai>`_. It can explain
individual directives in detail and suggest the modern equivalent.

How inputs are handled
======================

- **Ubuntu/Debian** usually load ``imjournal`` (reads from systemd’s journal).
- **RHEL/CentOS/Rocky/Alma** often use ``imuxsock`` (reads from the traditional syslog socket).
- Some distros load both for maximum compatibility.

That is why you should **not reload those same inputs again** in your snippets —
the distro already set them up.

But if you need to use a **new kind of input**, such as monitoring a text file
with ``imfile`` or receiving logs over TCP with ``imtcp``, then you *do* load
that module yourself. Adding new inputs is normal; reloading the already
configured system inputs is unnecessary.

Safe way to add your rules
==========================

- **Leave ``/etc/rsyslog.conf`` as it is.**
  Do not try to “modernize” the legacy lines — rsyslog understands them.

- **Add your own rules under ``/etc/rsyslog.d/*.conf``** in RainerScript syntax.
  Example:

  .. code-block:: rsyslog

     # Log all messages from facility 'local3' to a custom log file
     if ($syslogfacility-text == "local3") then {
         action(type="omfile" file="/var/log/myapp.log")
     }

Should you convert legacy lines?
================================

No — there is no need.
Over time you may choose to migrate, but rsyslog will happily run mixed syntax.

Verification checkpoint
=======================

By the end of this tutorial you should:

- Recognize legacy lines like ``*.* /var/log/syslog``.
- Understand why they exist in distro configs.
- Know that you should not remove or convert them.
- Be confident adding new modern rules in ``/etc/rsyslog.d/``.

See also / Next steps
=====================

- :doc:`02-first-config` – your first modern snippet.
- :doc:`04-message-pipeline` – learn how inputs, rulesets, and actions fit together.
- Existing page: :doc:`../understanding_default_config` – neutral reference version.

----

.. tip::
   🎬 *Video idea (2–3 min):* open ``/etc/rsyslog.conf``, highlight the mix of old
   and new lines, explain why it’s safe, then add a small snippet under
   ``/etc/rsyslog.d/`` to show the correct workflow.

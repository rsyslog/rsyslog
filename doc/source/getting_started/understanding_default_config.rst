Understanding the Default Configuration
=======================================

When you open ``/etc/rsyslog.conf`` on a freshly installed Linux system,
you might see a configuration that looks very different from the examples
in this guide. Lines such as ``$FileOwner`` or ``*.* /var/log/syslog``
can look unusual or even cryptic to new users.

This layout comes from how many Linux distributions package rsyslog.
It’s not the default style of rsyslog itself, but a compatibility choice
made by the distribution to keep older tools and setups working smoothly.

Why It Looks Different
----------------------

Distributions like Ubuntu, Debian, or RHEL ship rsyslog with a **mixed
configuration style**:

- **Legacy lines** such as ``$FileCreateMode`` or ``*.*`` are left over
  from older syslog systems.
- **Modern modules** (e.g., ``module(load="imuxsock")``) are included to
  add newer rsyslog features.

rsyslog supports both styles, so the system works out of the box. For
new configurations, we recommend **modern RainerScript syntax**, as shown
throughout this guide.

Why We Don’t Show ``*.*`` in Examples
-------------------------------------

In older configurations, you will often see a line like:

.. code-block:: none

   *.* /var/log/syslog

This means: *“Send all messages, regardless of facility or priority, to
/var/log/syslog.”*

Modern RainerScript configurations **do not need this line** because
logging everything to ``/var/log/syslog`` is already the default behavior.
You can add explicit filters if needed, but a ``*.*`` filter is no longer
necessary.

For example:

.. code-block:: rsyslog

   action(type="omfile" file="/var/log/syslog")

is enough to log all messages to ``/var/log/syslog``.

How to Work with the Default Config
-----------------------------------

- **Keep the distribution’s default files as they are.** They ensure that
  standard system logging works correctly.
- **Add your own rules in separate files** under ``/etc/rsyslog.d/``.
  This avoids conflicts with distro updates.

For example, create ``/etc/rsyslog.d/10-myapp.conf``:

.. code-block:: rsyslog

   # Log all messages from facility 'local3' to a custom log file.
   if ($syslogfacility-text == "local3") then {
       action(
           type="omfile"
           file="/var/log/myapp.log"
       )
   }

Should You Convert the Old Lines?
---------------------------------

No, you don’t need to convert them. rsyslog understands both styles.  
Over time, you might choose to migrate to RainerScript for clarity, but
it is not required.

Key Takeaways
-------------

- The default configuration is shaped by **distribution choices**, not by
  rsyslog itself.
- Modern RainerScript is easier to read and is used in all new examples.
- You can safely add your own rules in modern style without touching the
  existing legacy lines.


Basic Configuration
===================

rsyslog reads its main configuration from:

``/etc/rsyslog.conf``

Additional configuration snippets can be placed in:

``/etc/rsyslog.d/*.conf``

Minimal Example
---------------

The following configuration logs all messages to `/var/log/syslog`:

.. code-block:: rsyslog

   # Load the input modules for system and kernel logging.
   module(load="imuxsock")  # Local system logs (e.g., from journald)
   module(load="imklog")    # Kernel log capture

   # Traditionally, a *.* selector ("all logs") is added here.
   # This is unnecessary, as it is the default behavior.
   # Therefore, no filter is explicitly shown.
   action(type="omfile" file="/var/log/syslog")

Apply changes by restarting rsyslog:

.. code-block:: bash

   sudo systemctl restart rsyslog


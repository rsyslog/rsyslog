Installing rsyslog
==================

rsyslog is included by default in many Linux distributions. If it is
not installed, you can add it with the following commands.

On Debian/Ubuntu
----------------

.. code-block:: bash

   sudo apt update
   sudo apt install rsyslog rsyslog-doc

On RHEL/CentOS
--------------

.. code-block:: bash

   sudo dnf install rsyslog rsyslog-doc

Enable and Start the Service
----------------------------

After installation, enable and start rsyslog:

.. code-block:: bash

   sudo systemctl enable rsyslog
   sudo systemctl start rsyslog

Validating the Setup
--------------------

To verify your installation and configuration, run:

.. code-block:: bash

   rsyslogd -N1

This checks the configuration syntax without starting the daemon.


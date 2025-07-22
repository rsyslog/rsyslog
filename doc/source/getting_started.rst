Getting Started with rsyslog
----------------------------

rsyslog is a modern, high-performance logging framework that extends
traditional syslog functionality. It supports advanced features such as
structured logging, high-throughput message processing, and integration
with modern log pipelines (e.g., Elasticsearch, Kafka, cloud services).
rsyslog is actively maintained and widely used as the default system
logger on many Linux distributions.

Installation
~~~~~~~~~~~~

### On Debian/Ubuntu

.. code-block:: bash

   sudo apt update
   sudo apt install rsyslog rsyslog-doc

### On RHEL/CentOS

.. code-block:: bash

   sudo yum install rsyslog rsyslog-doc

After installation, enable and start rsyslog:

.. code-block:: bash

   sudo systemctl enable rsyslog
   sudo systemctl start rsyslog

Validating the Setup
~~~~~~~~~~~~~~~~~~~~

To verify your installation and configuration, run:

.. code-block:: bash

   rsyslogd -N1

This command checks configuration syntax without starting the daemon.

Basic Configuration
~~~~~~~~~~~~~~~~~~~

The primary configuration file is:

``/etc/rsyslog.conf``

Additional configuration snippets are placed in:

``/etc/rsyslog.d/*.conf``

Minimal Example
^^^^^^^^^^^^^^^

A simple configuration that logs all messages to `/var/log/syslog`:

.. code-block:: rsyslog

   module(load="imuxsock")  # Provides local system logging via Unix socket
   module(load="imklog")    # Enables kernel log capture
   
   # Traditionally, a *.* selector ("all logs") is added here.
   # This is unnecessary, as it is the default behavior.
   # Therefore, no filter is explicitly shown.
   action(type="omfile" file="/var/log/syslog")

Apply changes by restarting rsyslog:

.. code-block:: bash

   sudo systemctl restart rsyslog

First Advanced Step: Forwarding Logs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

rsyslog can forward logs to remote servers using TCP or TLS:

.. code-block:: rsyslog

   action(
       type="omfwd"
       protocol="tcp"
       target="logs.example.com"
       port="514"
   )

This configuration forwards all log messages to `logs.example.com`.

Modern Pipeline Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~

rsyslog supports many modern systems and formats, including:

- Elasticsearch and OpenSearch via the `omelasticsearch` module
- Kafka and other message brokers
- Structured logging with JSON templates

For example, to send logs to Elasticsearch:

.. code-block:: rsyslog

   module(load="omelasticsearch")

   action(
       type="omelasticsearch"
       server="http://localhost:9200"
       searchIndex="rsyslog"
   )

Next Steps
~---------

- Explore the :doc:`configuration/index` section for advanced settings
  and features.
- Review :doc:`tutorials/index` for step-by-step guides.
- For quick answers, try the `AI rsyslog assistant <https://rsyslog.ai>`_.

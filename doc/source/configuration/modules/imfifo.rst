*******************************
imfifo: Named Pipe Input Module
*******************************

.. index:: ! imfifo

.. meta::
   :description: Named pipe (FIFO) input module for rsyslog, reading logs line-by-line from FIFOs on the local filesystem.
   :keywords: rsyslog, imfifo, input, named pipe, fifo

.. summary-start

This module provides the ability to read log messages line-by-line from named pipes (FIFOs) on the local filesystem.

.. summary-end

===========================  ===========================================================================
**Module Name:**             **imfifo**
**Author/Maintainer:**       `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Introduced:**              8.2606.0
===========================  ===========================================================================

Purpose
=======

The `imfifo` module allows rsyslog to monitor local POSIX named pipes (FIFOs). It reads incoming log messages line-by-line and submits them to rsyslog queues for processing.

This module is designed for modern dynamic configuration and supports multiple input instances. This allows you to monitor different named pipes concurrently, each with different tag, ruleset, facility, or severity configurations.

Main Features
=============

- **Line-by-line Reading**: Messages are read from the named pipe and split on newline (``\n``) boundaries.
- **O_RDWR Opening Technique**: To prevent rsyslog from blocking indefinitely during startup when no writers are yet connected to the pipe, and to avoid spinning on EOF when a writer disconnects, the module opens named pipes in ``O_RDWR`` mode.
- **Ruleset Binding**: Each pipe instance can be bound to a specific ruleset for isolated log routing and processing.
- **Clean Shutdown**: Utilizes non-blocking select loops with a short timeout to ensure rsyslog shuts down cleanly and instantly when requested.

Configuration
=============

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imfifo-file`
     - .. include:: ../../reference/parameters/imfifo-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfifo-tag`
     - .. include:: ../../reference/parameters/imfifo-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfifo-facility`
     - .. include:: ../../reference/parameters/imfifo-facility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfifo-severity`
     - .. include:: ../../reference/parameters/imfifo-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfifo-ruleset`
     - .. include:: ../../reference/parameters/imfifo-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Configuration Examples
======================

The following example loads the ``imfifo`` module and configures two separate input instances to read from different local named pipes.

.. code-block:: rsyslog

   module(load="imfifo")

   # Monitor custom application pipe
   input(type="imfifo"
         file="/var/run/app_log.pipe"
         tag="app-pipe:"
         severity="info"
         facility="local3")

   # Monitor system events pipe
   input(type="imfifo"
         file="/var/run/sys_events.pipe"
         tag="sys-events:")

Notes
=====

- **Named Pipe Creation**: The target named pipes must be pre-created on the filesystem using `mkfifo` before rsyslog starts. For example:
  
  .. code-block:: bash

     mkfifo /var/run/app_log.pipe
     chmod 600 /var/run/app_log.pipe

- **Security & Posture**: To maintain a secure environment, ensure that the permissions on the named pipes are restricted (e.g., owned by the user running rsyslog and set to mode ``600`` or ``620``). Do not allow untrusted users to write to pipes monitored by rsyslog.

See also
========

- :doc:`../index`

.. toctree::
   :hidden:

   ../../reference/parameters/imfifo-facility
   ../../reference/parameters/imfifo-file
   ../../reference/parameters/imfifo-ruleset
   ../../reference/parameters/imfifo-severity
   ../../reference/parameters/imfifo-tag

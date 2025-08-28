*****************************************
omprog: Program integration Output module
*****************************************

===========================  ===========================================================================
**Module Name:**             **omprog**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module permits to integrate arbitrary external programs into
rsyslog's logging. It is similar to the "execute program (^)" action,
but offers better security and much higher performance. While "execute
program (^)" can be a useful tool for executing programs if rare events
occur, omprog can be used to provide massive amounts of log data to a
program.

Executes the configured program and feeds log messages to that binary
via stdin. The binary is free to do whatever it wants with the supplied
data. If the program terminates, it is re-started. If rsyslog
terminates, the program's stdin will see EOF. The program must then
terminate. The message format passed to the program can, as usual, be
modified by defining rsyslog templates.

Note that in order to execute the given program, rsyslog needs to have
sufficient permissions on the binary file. This is especially true if
not running as root. Also, keep in mind that default SELinux policies
most probably do not permit rsyslogd to execute arbitrary binaries. As
such, permissions must be appropriately added. Note that SELinux
restrictions also apply if rsyslogd runs under root. To check if a
problem is SELinux-related, you can temporarily disable SELinux and
retry. If it then works, you know for sure you have a SELinux issue.

Starting with 8.4.0, rsyslogd emits an error message via the ``syslog()``
API call when there is a problem executing the binary. This can be
extremely valuable in troubleshooting. For those technically savvy:
when we execute a binary, we need to fork, and we do not have
full access to rsyslog's usual error-reporting capabilities after the
fork. As the actual execution must happen after the fork, we cannot
use the default error logger to emit the error message. As such,
we use ``syslog()``. In most cases, there is no real difference
between both methods. However, if you run multiple rsyslog instances,
the message shows up in that instance that processes the default
log socket, which may be different from the one where the error occurred.
Also, if you redirected the log destination, that redirection may
not work as expected.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

.. toctree::
   :hidden:

   ../../reference/parameters/omprog-template
   ../../reference/parameters/omprog-binary
   ../../reference/parameters/omprog-confirmmessages
   ../../reference/parameters/omprog-confirmtimeout
   ../../reference/parameters/omprog-reportfailures
   ../../reference/parameters/omprog-usetransactions
   ../../reference/parameters/omprog-begintransactionmark
   ../../reference/parameters/omprog-committransactionmark
   ../../reference/parameters/omprog-output
   ../../reference/parameters/omprog-filecreatemode
   ../../reference/parameters/omprog-hup-signal
   ../../reference/parameters/omprog-signalonclose
   ../../reference/parameters/omprog-closetimeout
   ../../reference/parameters/omprog-killunresponsive
   ../../reference/parameters/omprog-forcesingleinstance

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omprog-template`
     - .. include:: ../../reference/parameters/omprog-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-binary`
     - .. include:: ../../reference/parameters/omprog-binary.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-confirmmessages`
     - .. include:: ../../reference/parameters/omprog-confirmmessages.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-confirmtimeout`
     - .. include:: ../../reference/parameters/omprog-confirmtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-reportfailures`
     - .. include:: ../../reference/parameters/omprog-reportfailures.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-usetransactions`
     - .. include:: ../../reference/parameters/omprog-usetransactions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-begintransactionmark`
     - .. include:: ../../reference/parameters/omprog-begintransactionmark.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-committransactionmark`
     - .. include:: ../../reference/parameters/omprog-committransactionmark.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-output`
     - .. include:: ../../reference/parameters/omprog-output.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-filecreatemode`
     - .. include:: ../../reference/parameters/omprog-filecreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-hup-signal`
     - .. include:: ../../reference/parameters/omprog-hup-signal.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-signalonclose`
     - .. include:: ../../reference/parameters/omprog-signalonclose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-closetimeout`
     - .. include:: ../../reference/parameters/omprog-closetimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-killunresponsive`
     - .. include:: ../../reference/parameters/omprog-killunresponsive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omprog-forcesingleinstance`
     - .. include:: ../../reference/parameters/omprog-forcesingleinstance.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Examples
========

Example: command line arguments
-------------------------------

In the following example, logs will be sent to a program ``log.sh`` located
in ``/usr/libexec/rsyslog``. The program will receive the command line arguments
``p1``, ``p2`` and ``--param3="value 3"``.

.. code-block:: none

   module(load="omprog")

   action(type="omprog"
          binary="/usr/libexec/rsyslog/log.sh p1 p2 --param3=\"value 3\""
          template="RSYSLOG_TraditionalFileFormat")


Example: external program that writes logs to a database
--------------------------------------------------------

In this example, logs are sent to the stdin of a Python program that
(let's assume) writes them to a database. A dedicated disk-assisted
queue with (a maximum of) 5 worker threads is used, to avoid affecting
other log destinations in moments of high load. The ``confirmMessages``
flag is enabled, which tells rsyslog to wait for the program to confirm
its initialization and each message received. The purpose of this setup
is preventing logs from being lost because of database connection
failures.

If the program cannot write a log to the database, it will return a
negative confirmation to rsyslog via stdout. Rsyslog will then keep the
failed log in the queue, and send it again to the program after 5
seconds. The program can also write error details to stderr, which will
be captured by rsyslog and written to ``/var/log/db_forward.log``. If
no response is received from the program within a 30-second timeout,
rsyslog will kill and restart it.

.. code-block:: none

   module(load="omprog")

   action(type="omprog"
          name="db_forward"
          binary="/usr/libexec/rsyslog/db_forward.py"
          confirmMessages="on"
          confirmTimeout="30000"
          queue.type="LinkedList"
          queue.saveOnShutdown="on"
          queue.workerThreads="5"
          action.resumeInterval="5"
          killUnresponsive="on"
          output="/var/log/db_forward.log")

Note that the ``useTransactions`` flag is not used in this example. The
program stores and confirms each log individually.



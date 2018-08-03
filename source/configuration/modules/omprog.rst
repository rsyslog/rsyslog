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
extremely valuable in troubleshooting. For those technically savy:
when we execute a binary, we need to fork, and we do not have
full access to rsyslog's usual error-reporting capabilities after the
fork. As the actual execution must happen after the fork, we cannot
use the default error logger to emit the error message. As such,
we use ``syslog()``. In most cases, there is no real difference
between both methods. However, if you run multiple rsyslog instances,
the message shows up in that instance that processes the default
log socket, which may be different from the one where the error occured.
Also, if you redirected the log destination, that redirection may
not work as expected.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "none"

Name of the :doc:`template <../templates>` to use to format the log messages
passed to the external program.


binary
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "", "yes", "``$ActionOMProgBinary``"

Full path and command line parameters of the external program to execute.

In legacy config, it is **not possible** to specify command line parameters.


.. _confirmMessages:

confirmMessages
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.31.0

Specifies whether the external program provides feedback to rsyslog via stdout.
When this switch is set to "on", rsyslog will wait for the program to confirm
each received message. This feature facilitates error handling: instead of
having to implement a retry logic, the external program can rely on the rsyslog
queueing capabilities.

To confirm a message, the program must write a line with the word ``OK`` to its
standard output. If it writes a line containing anything else, rsyslog considers
that the message could not be processed, keeps it in the action queue, and
re-sends it to the program later (after the period specified by the
:doc:`action.resumeInterval <../actions>` parameter).

In addition, when a new instance of the program is started, rsyslog will also
wait for the program to confirm it is ready to start consuming logs. This
prevents rsyslog from starting to send logs to a program that could not
complete its initialization properly.

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_


.. _useTransactions:

useTransactions
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.31.0

Specifies whether the external program processes the messages in
:doc:`batches <../../development/dev_oplugins>` (transactions). When this
switch is enabled, the logs sent to the program are grouped in transactions.
At the start of a transaction, rsyslog sends a special mark message to the
program (see beginTransactionMark_). At the end of the transaction, rsyslog
sends another mark message (see commitTransactionMark_).

If confirmMessages_ is also set to "on", the program must confirm both the
mark messages and the logs within the transaction. The mark messages must be
confirmed by returning ``OK``, and the individual messages by returning
``DEFER_COMMIT`` (instead of ``OK``). Refer to the link below for details. 

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_

.. warning::

   There is currently a `known issue
   <https://github.com/rsyslog/rsyslog/issues/2420>`_ with the use of
   transactions together with ``confirmMessages=on``.


.. _beginTransactionMark:

beginTransactionMark
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "BEGIN TRANSACTION", "no", "none"

.. versionadded:: 8.31.0

Allows specifying the mark message that rsyslog will send to the external
program to indicate the start of a transaction (batch). This parameter is
ignored if useTransactions_ is disabled.


.. _commitTransactionMark:

commitTransactionMark
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "COMMIT TRANSACTION", "no", "none"

.. versionadded:: 8.31.0

Allows specifying the mark message that rsyslog will send to the external
program to indicate the end of a transaction (batch). This parameter is
ignored if useTransactions_ is disabled.


.. _output:

output
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: v8.1.6

Full path of a file where the output of the external program must be saved,
for debugging purposes.

Note that if the action has multiple worker threads
(:doc:`queue.workerThreads <../../rainerscript/queue_parameters>` is
set to a value greater than 1), all threads will write to the file at the
same time, which will cause the output of the multiple child processes to be
mixed. When using this parameter, use a single worker thread.

If confirmMessages_ is set to "off" (the default), both the stdout and
stderr of the child process are written to the specified file.

If confirmMessages_ is set to "on", only the stderr of the child is
written to the specified file (since stdout is used for confirming the
messages).

.. warning::

   There is currently a known issue with omprog when this parameter is NOT
   used: the stderr of the program will be assigned arbitrarily, or closed,
   which can produce unpredictable results if the program emits something
   to stderr (`example <https://github.com/rsyslog/rsyslog/issues/2787>`_).
   As a workaround, it is recommended to explicitly redirect stderr within
   the program, or to use this parameter. In future versions, omprog will
   execute the program with stderr redirected to /dev/null when this
   parameter is not specified. The same considerations apply to stdout
   when confirmMessages_ is set to "off".


hup.signal
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.9.0

Specifies which signal, if any, is to be forwarded to the external program
when rsyslog receives a HUP signal. Currently, HUP, USR1, USR2, INT, and
TERM are supported. If unset, no signal is sent on HUP. This is the default
and what pre 8.9.0 versions did.


.. _signalOnClose:

signalOnClose
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.23.0

Specifies whether a TERM signal must be sent to the external program before
closing it (either because the worker thread has been unscheduled, or rsyslog
is about to shutdown).

If this switch is set to "on", rsyslog will send a TERM signal to the child
process before closing the pipe. That is, the process will first receive a
TERM signal, and then an EOF on stdin.

No signal is issued if this switch is set to "off" (default). The child
process can still detect it must terminate because reading from stdin will
return EOF.

See the killUnresponsive_ parameter for more details.


.. _closeTimeout:

closeTimeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "5000", "no", "none"

.. versionadded:: 8.35.0

Specifies how long rsyslog must wait for the external program to terminate
after closing the pipe (that is, sending EOF to the stdin of the child
process). The value must be expressed in milliseconds and must be greater
than or equal to zero.

See the killUnresponsive_ parameter for more details.


.. _killUnresponsive:

killUnresponsive
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "the value of 'signalOnClose'", "no", "none"

.. versionadded:: 8.35.0

Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by closeTimeout_ (when
rsyslog is shutting down or the worker thread is being unscheduled).

If signalOnClose_ is set to "on", the default value of ``killUnresponsive``
is also "on". In this case, the cleanup sequence of the child process is as
follows: (1) a TERM signal is sent to the child, (2) the pipe with the child
process is closed (the child will receive EOF on stdin), (3) rsyslog waits
for the child process to terminate during closeTimeout_, (4) if the child
has not terminated within the timeout, a KILL signal is sent to it.

If signalOnClose_ is set to "off", the default value of ``killUnresponsive``
is also "off". In this case, the child cleanup sequence is as follows: (1) the
pipe with the child process is closed (the child will receive EOF on stdin),
(2) rsyslog waits for the child process to terminate during closeTimeout_,
(3) if the child has not terminated within the timeout, rsyslog ignores it and
continues with the shutdown (or the unschedule of the worker thread).

This parameter can be set to a different value than signalOnClose_, obtaining
the corresponding variations of cleanup sequences described above.


forceSingleInstance
-------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: v8.1.6

By default, the omprog action will start an instance (process) of the
external program per worker thread (the maximum number of worker threads
can be specified with the
:doc:`queue.workerThreads <../../rainerscript/queue_parameters>`
parameter). Moreover, if the action is associated to a
:doc:`disk-assisted queue <../../concepts/queues>`, an additional instance
will be started when the queue is persisted, to process the items stored
on disk.

If you want to force a single instance of the program to be executed,
regardless of the number of worker threads or the queue type, set this
flag to "on". This is useful when the external program uses or accesses
some kind of shared resource that does not allow concurrent access from
multiple processes.

.. warning::

   This parameter is currently affected by `this issue
   <https://github.com/rsyslog/rsyslog/issues/2813>`_ that essentially
   causes it to have no effect. As a workaround, if you need this
   behavior, set the
   :doc:`queue.workerThreads <../../rainerscript/queue_parameters>`
   parameter to 1 (which is the default value), and do not use a
   disk-assisted queue for the omprog action.


Examples
========

Example: command line arguments
-------------------------------

In the following example, logs will be sent to a program ``log.sh`` located
in ``/path/to``. The program will receive the command line arguments
``-p="value 1"`` and ``--param2="value2"``.

.. code-block:: none

   module(load="omprog")

   action(type="omprog"
          binary="/path/to/log.sh -p=\"value 1\" --param2=\"value2\""
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
failed log  in the queue, and send it again to the program after 5
seconds, with infinite retries.

.. code-block:: none

   module(load="omprog")

   action(type="omprog"
          name="db_forward"
          binary="/usr/share/logging/db_forward.py"
          confirmMessages="on"
          queue.type="LinkedList"
          queue.saveOnShutdown="on"
          queue.workerThreads="5"
          action.resumeInterval="5"
          action.resumeRetryCount="-1")

Note that the ``useTransactions`` flag is not used in this example. The
program stores and confirms each log individually.


|FmtObsoleteName| directives
============================

-  **$ActionOMProgBinary** <binary>
   The binary program to be executed.

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
Arbitrary external programs should be placed under the /usr/libexec/rsyslog directory.
That is, the binaries put in this namespaced directory are meant for the consumption
of rsyslog, and are not intended to be executed by users.
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


.. _confirmTimeout:

confirmTimeout
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10000", "no", "none"

.. versionadded:: 8.38.0

Specifies how long rsyslog must wait for the external program to confirm
each message when confirmMessages_ is set to "on". If the program does not
send a response within this timeout, it will be restarted (see signalOnClose_,
closeTimeout_ and killUnresponsive_ for details on the cleanup sequence).
The value must be expressed in milliseconds and must be greater than zero.

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_


.. _reportFailures:

reportFailures
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.38.0

Specifies whether rsyslog must internally log a warning message whenever the
program returns an error when confirming a message. The logged message will
include the error line returned by the program. This parameter is ignored when
confirmMessages_ is set to "off".

Enabling this flag can be useful to log the problems detected by the program.
However, the information that can be logged is limited to a short error line,
and the logs will be tagged as originated by the 'syslog' facility (like the
rest of rsyslog logs). To avoid these shortcomings, consider the use of the
output_ parameter to capture the stderr of the program.


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

   This feature is currently **experimental**. It could change in future releases
   without keeping backwards compatibility with existing configurations or the
   specified interface. There is also a `known issue
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

Full path of a file where the output of the external program will be saved.
If the file already exists, the output is appended to it. If the file does
not exist, it is created with the permissions specified by fileCreateMode_.

If confirmMessages_ is set to "off" (the default), both the stdout and
stderr of the child process are written to the specified file.

If confirmMessages_ is set to "on", only the stderr of the child is
written to the specified file (since stdout is used for confirming the
messages).

Rsyslog will reopen the file whenever it receives a HUP signal. This allows
the file to be externally rotated (using a tool like *logrotate*): after
each rotation of the file, make sure a HUP signal is sent to rsyslogd.

If the omprog action is configured to use multiple worker threads
(:doc:`queue.workerThreads <../../rainerscript/queue_parameters>` is
set to a value greater than 1), the lines written by the various program
instances will not appear intermingled in the output file, as long as the
lines do not exceed a certain length and the program writes them to
stdout/stderr in line-buffered mode. For details, refer to `Interface between
rsyslog and external output plugins
<https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_.

If this parameter is not specified, the output of the program will be
redirected to ``/dev/null``.

.. note::

   Before version v8.38.0, this parameter was intended for debugging purposes
   only. Since v8.38.0, the parameter can be used for production.


.. _fileCreateMode:

fileCreateMode
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "0600", "no", "none"

.. versionadded:: v8.38.0

Permissions the output_ file will be created with, in case the file does not
exist. The value must be a 4-digit octal number, with the initial digit being
zero. Please note that the actual permission depends on the rsyslogd process
umask. If in doubt, use ``$umask 0000`` right at the beginning of the
configuration file to remove any restrictions.


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
closing it (when either the worker thread has been unscheduled, a restart
of the program is being forced, or rsyslog is about to shutdown).

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
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown) after closing the pipe,
that is, after sending EOF to the stdin of the child process. The value must
be expressed in milliseconds and must be greater than or equal to zero.

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
it does not terminate within the timeout indicated by closeTimeout_
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

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
(3) if the child has not terminated within the timeout, rsyslog ignores it.

This parameter can be set to a different value than signalOnClose_, obtaining
the corresponding variations of cleanup sequences described above.


forceSingleInstance
^^^^^^^^^^^^^^^^^^^

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

.. note::

   Before version v8.38.0, this parameter had no effect.


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


|FmtObsoleteName| directives
============================

-  **$ActionOMProgBinary** <binary>
   The binary program to be executed.

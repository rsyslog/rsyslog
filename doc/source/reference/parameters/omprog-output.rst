.. _param-omprog-output:
.. _omprog.parameter.action.output:

output
======

.. index::
   single: omprog; output
   single: output

.. summary-start

Appends the program's output streams to the specified file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: output
:Scope: action
:Type: string
:Default: action=none
:Required?: no
:Introduced: v8.1.6

Description
-----------
Full path of a file where the output of the external program will be saved.
If the file already exists, the output is appended to it. If the file does
not exist, it is created with the permissions specified by :ref:`param-omprog-filecreatemode`.

If :ref:`param-omprog-confirmmessages` is set to "off" (the default), both the stdout and
stderr of the child process are written to the specified file.

If :ref:`param-omprog-confirmmessages` is set to "on", only the stderr of the child is
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

Action usage
------------
.. _param-omprog-action-output:
.. _omprog.parameter.action.output-usage:

.. code-block:: rsyslog

   action(type="omprog" output="/var/log/prog.log")

See also
--------
See also :doc:`../../configuration/modules/omprog`.

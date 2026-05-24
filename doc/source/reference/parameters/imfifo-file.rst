.. _param-imfifo-file:
.. _imfifo.parameter.input.file:

file
====

.. meta::
   :description: Path to the named pipe (FIFO) to monitor.
   :keywords: rsyslog, imfifo, file, pipe

.. index::
   single: imfifo; file
   single: file

.. summary-start

Specifies the absolute filesystem path of the named pipe (FIFO) to read log messages from.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfifo`.

:Name: file
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: yes
:Introduced: 8.2606.0

Description
-----------
The path to the named pipe (FIFO) to be monitored. This must be an absolute path.
The FIFO must exist on the filesystem before Rsyslog starts; `imfifo` will not attempt to create it.
If the file does not exist or is not a FIFO, an error will be reported at startup and the input instance will not be activated.

Input usage
-----------
.. _param-imfifo-input-file:
.. _imfifo.parameter.input.file-usage:

.. code-block:: rsyslog

   input(type="imfifo"
         file="/var/log/myfifo"
         tag="fifo-logs:")

See also
--------
See also :doc:`../../configuration/modules/imfifo`.

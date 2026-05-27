.. _param-imfifo-severity:
.. _imfifo.parameter.input.severity:

severity
========

.. meta::
   :description: Syslog severity to apply to messages read from this named pipe (FIFO).
   :keywords: rsyslog, imfifo, severity

.. index::
   single: imfifo; severity
   single: severity

.. summary-start

Specifies the syslog severity to apply to messages read from this named pipe (FIFO).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfifo`.

:Name: severity
:Scope: input
:Type: severity (word/integer)
:Default: notice
:Required?: no
:Introduced: 8.2606.0

Description
-----------
The syslog severity to be assigned to all messages read from this named pipe. It can be specified in textual form (for example ``notice``) or as a number (for example ``5``). Textual form is recommended.

Input usage
-----------
.. _param-imfifo-input-severity:
.. _imfifo.parameter.input.severity-usage:

.. code-block:: rsyslog

   input(type="imfifo"
         file="/var/log/myfifo"
         tag="fifo-logs:"
         severity="info")

See also
--------
See also :doc:`../../configuration/modules/imfifo`.

.. _param-imfifo-facility:
.. _imfifo.parameter.input.facility:

facility
========

.. meta::
   :description: Syslog facility to apply to messages read from this named pipe (FIFO).
   :keywords: rsyslog, imfifo, facility

.. index::
   single: imfifo; facility
   single: facility

.. summary-start

Specifies the syslog facility to apply to messages read from this named pipe (FIFO).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfifo`.

:Name: facility
:Scope: input
:Type: facility (word/integer)
:Default: local0
:Required?: no
:Introduced: 8.2606.0

Description
-----------
The syslog facility to be assigned to all messages read from this named pipe. It can be specified in textual form (for example ``local0``) or as a number (for example ``16``). Textual form is recommended.

Input usage
-----------
.. _param-imfifo-input-facility:
.. _imfifo.parameter.input.facility-usage:

.. code-block:: rsyslog

   input(type="imfifo"
         file="/var/log/myfifo"
         tag="fifo-logs:"
         facility="local4")

See also
--------
See also :doc:`../../configuration/modules/imfifo`.

.. _param-imfifo-tag:
.. _imfifo.parameter.input.tag:

tag
===

.. meta::
   :description: Syslog tag to apply to messages read from this named pipe (FIFO).
   :keywords: rsyslog, imfifo, tag

.. index::
   single: imfifo; tag
   single: tag

.. summary-start

Specifies the syslog tag to apply to messages read from this named pipe (FIFO).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfifo`.

:Name: tag
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: yes
:Introduced: 8.2606.0

Description
-----------
The syslog tag to be assigned to all messages read from this named pipe. The tag is typically used to identify the source of the logs in later filtering or formatting steps.

Input usage
-----------
.. _param-imfifo-input-tag:
.. _imfifo.parameter.input.tag-usage:

.. code-block:: rsyslog

   input(type="imfifo"
         file="/var/log/myfifo"
         tag="fifo-logs:")

See also
--------
See also :doc:`../../configuration/modules/imfifo`.

.. _param-imuxsock-usepidfromsystem:
.. _imuxsock.parameter.input.usepidfromsystem:

UsePIDFromSystem
================

.. index::
   single: imuxsock; UsePIDFromSystem
   single: UsePIDFromSystem

.. summary-start

Obtains the process ID from the log socket instead of the message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: UsePIDFromSystem
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies if the pid being logged shall be obtained from the log socket
itself. If so, the TAG part of the message is rewritten. It is
recommended to turn this option on, but the default is "off" to keep
compatible with earlier versions of rsyslog.

Input usage
-----------
.. _param-imuxsock-input-usepidfromsystem:
.. _imuxsock.parameter.input.usepidfromsystem-usage:

.. code-block:: rsyslog

   input(type="imuxsock" usePidFromSystem="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.inputunixlistensocketusepidfromsystem:

- $InputUnixListenSocketUsePIDFromSystem — maps to UsePIDFromSystem (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketUsePIDFromSystem
   single: $InputUnixListenSocketUsePIDFromSystem

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.

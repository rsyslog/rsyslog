.. _param-imuxsock-hostname:
.. _imuxsock.parameter.input.hostname:

HostName
========

.. index::
   single: imuxsock; HostName
   single: HostName

.. summary-start

Overrides the hostname used inside messages from this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: HostName
:Scope: input
:Type: string
:Default: input=NULL
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Allows overriding the hostname that shall be used inside messages
taken from the input that is being defined.

Input usage
-----------
.. _param-imuxsock-input-hostname:
.. _imuxsock.parameter.input.hostname-usage:

.. code-block:: rsyslog

   input(type="imuxsock" hostName="jail1.example.net")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.inputunixlistensockethostname:
- $InputUnixListenSocketHostName — maps to HostName (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketHostName
   single: $InputUnixListenSocketHostName

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.

.. _param-imdiag-listenportfilename:
.. _imdiag.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imdiag; ListenPortFileName
   single: ListenPortFileName

.. summary-start

Writes the port chosen for the diagnostic listener to the named file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ListenPortFileName
:Scope: input
:Type: string (filename)
:Default: input=none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies a file that receives the TCP port number after
:ref:`ServerRun <param-imdiag-serverrun>` initializes the listener. The
parameter is mandatory—imdiag duplicates the value internally and requires it
before the listener is created.

The file is overwritten each time the listener starts. When the listener is
configured with port ``0`` (ephemeral port selection), this file is the only way
to discover the actual port allocated by the operating system.

Input usage
-----------
.. _param-imdiag-input-listenportfilename:
.. _imdiag.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port" serverRun="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiaglistenportfilename:

- $IMDiagListenPortFileName — maps to ListenPortFileName (status: legacy)

.. index::
   single: imdiag; $IMDiagListenPortFileName
   single: $IMDiagListenPortFileName

See also
--------
See also :doc:`../../configuration/modules/imdiag`.

.. _param-imdiag-serverrun:
.. _imdiag.parameter.input.serverrun:

ServerRun
=========

.. index::
   single: imdiag; ServerRun
   single: ServerRun

.. summary-start

Creates the imdiag TCP listener on the specified port (``0`` selects an ephemeral port).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ServerRun
:Scope: input
:Type: integer (port)
:Default: input=none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Starts the diagnostic control listener. The value is the TCP port number to
bind. Specify ``0`` to request an ephemeral port from the operating system; the
chosen port is written to :ref:`ListenPortFileName <param-imdiag-listenportfilename>`.

imdiag always selects the plain TCP (``ptcp``) network stream driver. As a
result parameters that normally tune stream driver authentication or permitted
peers are accepted for compatibility but have no effect in current releases.

imdiag supports only a single listener. Attempting to configure ``ServerRun``
more than once logs an error and the additional configuration is ignored. All
module-level parameters (for example :ref:`MaxSessions <param-imdiag-maxsessions>`)
must be set before invoking ``ServerRun``.

Input usage
-----------
.. _param-imdiag-input-serverrun:
.. _imdiag.parameter.input.serverrun-usage:

.. code-block:: rsyslog

   module(load="imdiag" maxSessions="20")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port" serverRun="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagserverrun:

- $IMDiagServerRun — maps to ServerRun (status: legacy)

.. index::
   single: imdiag; $IMDiagServerRun
   single: $IMDiagServerRun

See also
--------
See also :doc:`../../configuration/modules/imdiag`.

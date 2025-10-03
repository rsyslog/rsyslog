.. _param-imdiag-serverstreamdrivermode:
.. _imdiag.parameter.input.serverstreamdrivermode:

ServerStreamDriverMode
======================

.. index::
   single: imdiag; ServerStreamDriverMode
   single: ServerStreamDriverMode

.. summary-start

Sets the mode value passed to the configured network stream driver.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ServerStreamDriverMode
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Provides the mode number consumed by the selected
:doc:`network stream driver <../../concepts/netstrm_drvr>`. The meaning of the
value is driver-specific. Configure this parameter before invoking
:ref:`ServerRun <param-imdiag-serverrun>` so that the listener picks up the
desired transport behavior (for example, TLS wrapper modes).

Input usage
-----------
.. _param-imdiag-input-serverstreamdrivermode:
.. _imdiag.parameter.input.serverstreamdrivermode-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
         serverStreamDriverMode="1" serverRun="19998")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagserverstreamdrivermode:

- $IMDiagServerStreamDriverMode — maps to ServerStreamDriverMode (status: legacy)

.. index::
   single: imdiag; $IMDiagServerStreamDriverMode
   single: $IMDiagServerStreamDriverMode

See also
--------
See also :doc:`../../configuration/modules/imdiag`.

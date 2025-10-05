.. _param-imdiag-serverstreamdrivermode:
.. _imdiag.parameter.input.serverstreamdrivermode:

ServerStreamDriverMode
======================

.. index::
   single: imdiag; ServerStreamDriverMode
   single: ServerStreamDriverMode

.. summary-start

Accepts a numeric stream driver mode value, but imdiag forces the
plain TCP driver so the setting is ignored.

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
:doc:`network stream driver <../../concepts/netstrm_drvr>`. imdiag always uses
the plain TCP (``ptcp``) stream driver, which does not act on the provided
mode value. The parameter remains available for configuration compatibility and
possible future extensions, but it does not alter behavior in current
releases.

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

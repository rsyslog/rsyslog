.. _param-imdiag-serverstreamdriverauthmode:
.. _imdiag.parameter.input.serverstreamdriverauthmode:

ServerStreamDriverAuthMode
==========================

.. index::
   single: imdiag; ServerStreamDriverAuthMode
   single: ServerStreamDriverAuthMode

.. summary-start

Accepts a stream driver authentication mode string, but imdiag always
uses the plain TCP driver so the value has no effect.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ServerStreamDriverAuthMode
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Selects the authentication mode for the active
:doc:`network stream driver <../../concepts/netstrm_drvr>`. imdiag
unconditionally binds to the plain TCP driver (``ptcp``) and therefore lacks
TLS or other authenticated stream implementations. The value is accepted for
compatibility with the generic TCP listener framework but is ignored by the
``ptcp`` driver.

Configure this parameter before :ref:`ServerRun <param-imdiag-serverrun>` if you
need forward compatibility with a future build that supports alternate stream
drivers. In current releases the setting does not change listener behavior.

Input usage
-----------
.. _param-imdiag-input-serverstreamdriverauthmode:
.. _imdiag.parameter.input.serverstreamdriverauthmode-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
         serverStreamDriverAuthMode="anon" serverRun="19998")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagserverstreamdriverauthmode:

- $IMDiagServerStreamDriverAuthMode — maps to ServerStreamDriverAuthMode (status: legacy)

.. index::
   single: imdiag; $IMDiagServerStreamDriverAuthMode
   single: $IMDiagServerStreamDriverAuthMode

See also
--------
See also :doc:`../../configuration/modules/imdiag`.

.. _param-imdiag-serverstreamdriverauthmode:
.. _imdiag.parameter.input.serverstreamdriverauthmode:

ServerStreamDriverAuthMode
==========================

.. index::
   single: imdiag; ServerStreamDriverAuthMode
   single: ServerStreamDriverAuthMode

.. summary-start

Chooses the authentication mode for the configured stream driver.

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
:doc:`network stream driver <../../concepts/netstrm_drvr>`. The accepted values
are driver-specific (for example, ``anon`` or ``x509/name`` when using a TLS
stream driver). Configure this parameter before :ref:`ServerRun
<param-imdiag-serverrun>` to ensure the listener starts with the intended
transport security.

Input usage
-----------
.. _param-imdiag-input-serverstreamdriverauthmode:
.. _imdiag.parameter.input.serverstreamdriverauthmode-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
         serverStreamDriverAuthMode="x509/name" serverRun="19998")

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

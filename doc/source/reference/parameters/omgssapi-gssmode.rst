.. _param-omgssapi-gssmode:
.. _omgssapi.parameter.module.gssmode:

GssMode
=======

.. index::
   single: omgssapi; GssMode
   single: GssMode

.. summary-start

Chooses whether omgssapi requests integrity-only or encrypted message protection for GSSAPI sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omgssapi`.

:Name: GssMode
:Scope: module
:Type: string
:Default: module=encryption
:Required?: no
:Introduced: 1.21.2

Description
-----------
Specifies the GSS-API protection level used by the client. The available modes are:

- ``integrity`` — only authenticates clients and verifies message integrity.
- ``encryption`` — provides the same guarantees as ``integrity`` and encrypts
  messages when both peers support it.

Legacy configurations set this directive with statements such as
``$GssMode Encryption``.

Module usage
------------
.. _param-omgssapi-module-gssmode:
.. _omgssapi.parameter.module.gssmode-usage:

.. code-block:: rsyslog

   module(load="omgssapi" gssMode="encryption")
   action(type="omgssapi" target="receiver.mydomain.com")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omgssapi.parameter.legacy.gssmode:

- $GssMode — maps to GssMode (status: legacy)

.. index::
   single: omgssapi; $GssMode
   single: $GssMode

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.

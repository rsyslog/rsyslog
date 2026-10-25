.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the omgssapi GssMode legacy directive.
   :keywords: rsyslog, omgssapi, GssMode, legacy configuration

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
:Default: encryption
:Required?: no
:Introduced: 1.21.2

Description
-----------
Specifies the GSS-API protection level used by the client. The available modes are:

- ``integrity`` — only authenticates clients and verifies message integrity.
- ``encryption`` — provides the same guarantees as ``integrity`` and encrypts
  messages when both peers support it.

Legacy configurations set this directive with statements such as
``$GssMode encryption``.

Module usage
------------
.. _param-omgssapi-module-gssmode:
.. _omgssapi.parameter.module.gssmode-usage:

This module supports legacy directives and selector actions. Load it before
setting the directive, then configure the forwarding action.

.. code-block:: none

   module(load="omgssapi")
   $GssMode encryption
   *.* :omgssapi:receiver.mydomain.com

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The following directive is the supported configuration interface for omgssapi.

.. _omgssapi.parameter.legacy.gssmode:

- ``$GssMode`` — sets GssMode for subsequent forwarding actions.

.. index::
   single: omgssapi; $GssMode
   single: $GssMode

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.

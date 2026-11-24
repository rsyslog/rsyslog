.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the omgssapi GssTokenIOTimeout legacy directive.
   :keywords: rsyslog, omgssapi, GssTokenIOTimeout, legacy configuration

.. _param-omgssapi-gsstokeniotimeout:
.. _omgssapi.parameter.module.gsstokeniotimeout:

GssTokenIOTimeout
=================

.. index::
   single: omgssapi; GssTokenIOTimeout
   single: GssTokenIOTimeout

.. summary-start

Sets the deadline in seconds for reading a full GSSAPI handshake token while omgssapi establishes a session.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omgssapi`.

:Name: GssTokenIOTimeout
:Scope: module
:Type: integer
:Default: 10
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Controls how long omgssapi waits for each full GSSAPI handshake token while it
initializes a secured forwarding session. The timeout covers both the 4-byte
token length and the token payload.

Increase this value if the receiver is reachable only over slow or high-latency
links. Set it to ``0`` to disable the deadline entirely. Negative values are
rejected as configuration errors.

Module usage
------------
.. _param-omgssapi-module-gsstokeniotimeout:
.. _omgssapi.parameter.module.gsstokeniotimeout-usage:

This module supports legacy directives and selector actions. Load it before
setting the directive, then configure the forwarding action.

.. code-block:: none

   module(load="omgssapi")
   $GssTokenIOTimeout 30
   *.* :omgssapi:receiver.mydomain.com

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The following directive is the supported configuration interface for omgssapi.

.. _omgssapi.parameter.legacy.gsstokeniotimeout:

- ``$GssTokenIOTimeout`` — sets GssTokenIOTimeout for subsequent forwarding actions.

.. index::
   single: omgssapi; $GssTokenIOTimeout
   single: $GssTokenIOTimeout

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.

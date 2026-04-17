.. _param-imgssapi-inputgssservertokeniotimeout:
.. _imgssapi.parameter.input.inputgssservertokeniotimeout:

InputGSSServerTokenIOTimeout
============================

.. index::
   single: imgssapi; InputGSSServerTokenIOTimeout
   single: InputGSSServerTokenIOTimeout

.. summary-start

Sets the deadline in seconds for reading a full GSSAPI token on an imgssapi session.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerTokenIOTimeout
:Scope: input
:Type: integer
:Default: 10
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Controls how long imgssapi waits to receive an entire GSSAPI token, including
the 4-byte token length and the token payload. The timeout applies to GSSAPI
handshake tokens as well as protected message frames.

Set this value higher on slow or high-latency links. Set it to ``0`` to disable
the deadline entirely. Negative values are rejected as configuration errors.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssservertokeniotimeout-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSServerTokenIOTimeout 30

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssservertokeniotimeout:

- $InputGSSServerTokenIOTimeout — maps to InputGSSServerTokenIOTimeout (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerTokenIOTimeout
   single: $InputGSSServerTokenIOTimeout

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.

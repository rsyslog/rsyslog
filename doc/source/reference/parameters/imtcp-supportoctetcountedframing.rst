.. _param-imtcp-supportoctetcountedframing:
.. _imtcp.parameter.input.supportoctetcountedframing:

SupportOctetCountedFraming
==========================

.. index::
   single: imtcp; SupportOctetCountedFraming
   single: SupportOctetCountedFraming

.. summary-start

Enables or disables support for legacy octet-counted framing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: SupportOctetCountedFraming
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If set to "on", the legacy octet-counted framing (similar to RFC 5425 framing) is activated. This should generally be left unchanged. It may be useful to turn it off if you know this framing is not used and some senders emit multi-line messages into the message stream.

Input usage
-----------
.. _imtcp.parameter.input.supportoctetcountedframing-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" supportOctetCountedFraming="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserversupportoctetcountedframing:
- $InputTCPServerSupportOctetCountedFraming — maps to SupportOctetCountedFraming (status: legacy)

.. index::
   single: imtcp; $InputTCPServerSupportOctetCountedFraming
   single: $InputTCPServerSupportOctetCountedFraming

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

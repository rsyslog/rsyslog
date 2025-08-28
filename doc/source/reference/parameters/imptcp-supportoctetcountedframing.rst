.. _param-imptcp-supportoctetcountedframing:
.. _imptcp.parameter.input.supportoctetcountedframing:

SupportOctetCountedFraming
==========================

.. index::
   single: imptcp; SupportOctetCountedFraming
   single: SupportOctetCountedFraming

.. summary-start

Enables legacy octet-counted framing similar to RFC5425.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: SupportOctetCountedFraming
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The legacy octet-counted framing (similar to RFC5425 framing) is
activated. This is the default and should be left unchanged until you
know very well what you do. It may be useful to turn it off, if you know
this framing is not used and some senders emit multi-line messages into
the message stream.

Input usage
-----------
.. _param-imptcp-input-supportoctetcountedframing:
.. _imptcp.parameter.input.supportoctetcountedframing-usage:

.. code-block:: rsyslog

   input(type="imptcp" supportOctetCountedFraming="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserversupportoctetcountedframing:

- $InputPTCPServerSupportOctetCountedFraming — maps to SupportOctetCountedFraming (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerSupportOctetCountedFraming
   single: $InputPTCPServerSupportOctetCountedFraming

See also
--------
See also :doc:`../../configuration/modules/imptcp`.

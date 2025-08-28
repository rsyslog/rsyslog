.. _param-imtcp-supportoctetcountedframing:
.. _imtcp.parameter.input.supportoctetcountedframing:

SupportOctetCountedFraming
==========================

.. index::
   single: imtcp; SupportOctetCountedFraming
   single: SupportOctetCountedFraming

.. summary-start

Enables legacy octet-counted framing compatibility.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: SupportOctetCountedFraming
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If set to ``on``, the legacy octet-counted framing (similar to RFC5425 framing) is activated.
This should be left unchanged until you know very well what you do. It may be useful to turn it
off if this framing is not used and some senders emit multi-line messages into the message stream.

Input usage
-----------
.. _param-imtcp-input-supportoctetcountedframing:
.. _imtcp.parameter.input.supportoctetcountedframing-usage:

.. code-block:: rsyslog

   input(type="imtcp" supportOctetCountedFraming="off")

Notes
-----
- Earlier documentation described the type as "binary"; this maps to boolean.

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

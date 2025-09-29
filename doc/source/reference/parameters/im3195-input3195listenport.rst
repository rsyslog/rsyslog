.. _param-im3195-input3195listenport:
.. _im3195.parameter.input.input3195listenport:

Input3195ListenPort
===================

.. index::
   single: im3195; Input3195ListenPort
   single: Input3195ListenPort

.. summary-start

Listens for RFC 3195 messages on the specified TCP port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/im3195`.

:Name: Input3195ListenPort
:Scope: input
:Type: integer
:Default: input=601
:Required?: no
:Introduced: Not documented

Description
-----------

.. note::

   This parameter is only available in the legacy configuration format.

The port on which im3195 listens for RFC 3195 messages. The default port is
601 (the IANA-assigned port).

Input usage
-----------
.. _param-im3195-input-input3195listenport:
.. _im3195.parameter.input.input3195listenport-usage:

.. code-block:: rsyslog

   $Input3195ListenPort 1601

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _im3195.parameter.legacy.input3195listenport:

- $Input3195ListenPort — maps to Input3195ListenPort (status: legacy)

.. index::
   single: im3195; $Input3195ListenPort
   single: $Input3195ListenPort

See also
--------
See also :doc:`../../configuration/modules/im3195`.

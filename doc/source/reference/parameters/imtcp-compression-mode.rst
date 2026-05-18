.. _param-imtcp-compression-mode:
.. _imtcp.parameter.input.compression-mode:

.. meta::
   :description: Reference for the imtcp compression.mode input parameter.
   :keywords: rsyslog, imtcp, compression.mode, stream compression, decompression, zlib, zstd

compression.mode
================

.. index::
   single: imtcp; compression.mode
   single: compression.mode

.. summary-start

Enables stream decompression for TCP streams sent by omfwd with
``compression.mode="stream:always"``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: compression.mode
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``compression.mode`` selects whether ``imtcp`` decompresses the received TCP
stream before parsing syslog frames.

Accepted values are:

- ``none``: do not attempt stream decompression.
- ``stream:always``: treat the full TCP byte stream as compressed data from
  :doc:`omfwd <../../configuration/modules/omfwd>` configured with
  ``compression.mode="stream:always"``.

The sender and receiver must use the same ``compression.driver``. The default
driver is ``zlib``. For ``zstd``, both sides must be built with libzstd support
and configured with ``compression.driver="zstd"``.

This is rsyslog stream compression, not TLS-native compression. It is intended
for trusted company VPNs and internal WAN/LAN links where the endpoints are
controlled together. Compression can create side channels through compressed
sizes and timing, so do not enable it for sensitive traffic unless that risk is
acceptable. Use TLS separately when confidentiality or peer authentication is
required.

``imtcp`` supports ``stream:always`` decompression only. It does not receive
``omfwd`` sender-side ``single`` compression mode.

Input usage
-----------
.. _param-imtcp-input-compression-mode:
.. _imtcp.parameter.input.compression-mode-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" compression.mode="stream:always")

See also
--------

See also :ref:`param-imtcp-compression-driver` and
:doc:`../../configuration/modules/imtcp`.

.. SPDX-License-Identifier: Apache-2.0

.. _param-imptcp-compression-maxexpansionratio:
.. _imptcp.parameter.input.compression-maxexpansionratio:

.. meta::
   :description: Reference for the imptcp compression.maxExpansionRatio input parameter.
   :keywords: rsyslog, imptcp, compression.maxExpansionRatio, stream compression, decompression

compression.maxExpansionRatio
=============================

.. index::
   single: imptcp; compression.maxExpansionRatio
   single: compression.maxExpansionRatio

.. summary-start

Limits the cumulative decompressed-to-compressed byte ratio accepted for a stream.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: compression.maxExpansionRatio
:Scope: input
:Type: integer
:Default: input=1024
:Required?: no
:Introduced: 8.2612.0

Description
-----------

``compression.maxExpansionRatio`` limits how much decompressed output
``imptcp`` accepts relative to the compressed bytes received on one session
when ``compression.mode`` selects stream decompression (``stream:always``, or
``stream:auto`` for sessions detected as compressed). The default allows up to
1024 decompressed bytes per compressed byte.

If the ratio is exceeded, ``imptcp`` logs the compressed stream as invalid,
discards the remaining stream data, and closes the session. This bounds
decompression amplification from malformed or unexpectedly dense compressed
streams. Set the value to ``0`` to disable this ratio limit for tightly
controlled deployments.

The parameter behaves like the identically named :doc:`imtcp
<../../configuration/modules/imtcp>` parameter.

Input usage
-----------
.. _param-imptcp-input-compression-maxexpansionratio:
.. _imptcp.parameter.input.compression-maxexpansionratio-usage:

.. code-block:: rsyslog

   input(
       type="imptcp"
       port="514"
       compression.mode="stream:always"
       compression.maxExpansionRatio="2048"
   )

See also
--------

See also :ref:`param-imptcp-compression-mode`,
:ref:`param-imptcp-compression-maxdecompressedbytesperreceive`, and
:doc:`../../configuration/modules/imptcp`.

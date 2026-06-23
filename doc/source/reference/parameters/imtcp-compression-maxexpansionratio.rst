.. _param-imtcp-compression-maxexpansionratio:
.. _imtcp.parameter.input.compression-maxexpansionratio:

.. meta::
   :description: Reference for the imtcp compression.maxExpansionRatio input parameter.
   :keywords: rsyslog, imtcp, compression.maxExpansionRatio, stream compression, decompression, security

compression.maxExpansionRatio
=============================

.. index::
   single: imtcp; compression.maxExpansionRatio
   single: compression.maxExpansionRatio

.. summary-start

Limits the cumulative decompressed-to-compressed byte ratio accepted for a stream.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: compression.maxExpansionRatio
:Scope: input/module
:Type: integer
:Default: input=1024
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``compression.maxExpansionRatio`` limits how much decompressed output ``imtcp``
accepts relative to compressed bytes received across the compressed stream when
``compression.mode="stream:always"`` is enabled. The default allows up to 1024
decompressed bytes per compressed byte.

If the ratio is exceeded, ``imtcp`` logs the compressed stream as invalid and
closes the session. This bounds decompression amplification from malformed or
unexpectedly dense compressed streams. Set the value to ``0`` to disable this
ratio limit for tightly controlled deployments.

Input usage
-----------
.. _param-imtcp-input-compression-maxexpansionratio:
.. _imtcp.parameter.input.compression-maxexpansionratio-usage:

.. code-block:: rsyslog

   input(
       type="imtcp"
       port="514"
       compression.mode="stream:always"
       compression.maxExpansionRatio="2048"
   )

See also
--------

See also :ref:`param-imtcp-compression-mode`,
:ref:`param-imtcp-compression-maxdecompressedbytesperreceive`, and
:doc:`../../configuration/modules/imtcp`.

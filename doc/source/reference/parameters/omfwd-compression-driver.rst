.. _param-omfwd-compression-driver:
.. _omfwd.parameter.action.compression-driver:

.. meta::
   :description: Reference for the omfwd compression.driver action parameter.
   :keywords: rsyslog, omfwd, compression.driver, stream compression, zlib, zstd

compression.driver
==================

.. index::
   single: omfwd; compression.driver
   single: compression.driver

.. summary-start

Selects the compression driver used by ``omfwd`` stream compression.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfwd`.

:Name: compression.driver
:Scope: action
:Type: word
:Default: action=zlib
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``compression.driver`` selects the compression library used when
``compression.mode="stream:always"`` is configured. It does not enable
compression by itself; set ``compression.mode`` and a non-zero ``zipLevel`` as
needed for the forwarding action.

Accepted values are:

- ``zlib``: default stream compression driver.
- ``zstd``: zstd stream compression driver. This requires rsyslog to be built
  with libzstd support, and the receiver must also use
  ``compression.driver="zstd"``.

The receiver must use the same driver. For ``imtcp``, configure
``compression.mode="stream:always"`` and the matching
``compression.driver`` on the input.

Action usage
------------
.. _param-omfwd-action-compression-driver:
.. _omfwd.parameter.action.compression-driver-usage:

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="logs.example.com"
       port="514"
       protocol="tcp"
       compression.mode="stream:always"
       compression.driver="zstd"
       zipLevel="3"
   )

See also
--------

See also :doc:`../../configuration/modules/omfwd` and
:doc:`../../configuration/modules/imtcp`.

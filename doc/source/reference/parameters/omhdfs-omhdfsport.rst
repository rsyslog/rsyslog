.. _param-omhdfs-omhdfsport:
.. _omhdfs.parameter.module.omhdfsport:

omhdfsPort
==========

.. index::
   single: omhdfs; omhdfsPort
   single: omhdfsPort

.. summary-start

Specifies the TCP port number used to reach the configured HDFS host.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: omhdfsPort
:Scope: module
:Type: integer
:Default: 0
:Required?: no
:Introduced: Not documented

Description
-----------
Port on which to connect to the HDFS host.

Module usage
------------
.. _param-omhdfs-module-omhdfsport:
.. _omhdfs.parameter.module.omhdfsport-usage:

.. code-block:: rsyslog

   $ModLoad omhdfs
   $omhdfsPort 8020

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfsport:

- $OMHDFSPort — maps to omhdfsPort (status: legacy)

.. index::
   single: omhdfs; $OMHDFSPort
   single: $OMHDFSPort

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

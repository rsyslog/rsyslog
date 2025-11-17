.. _param-omhdfs-omhdfshost:
.. _omhdfs.parameter.module.omhdfshost:

omhdfsHost
==========

.. index::
   single: omhdfs; omhdfsHost
   single: omhdfsHost

.. summary-start

Selects the hostname or IP address of the HDFS system omhdfs connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: omhdfsHost
:Scope: module
:Type: word
:Default: default
:Required?: no
:Introduced: Not documented

Description
-----------
Name or IP address of the HDFS host to connect to.

Module usage
------------
.. _omhdfs.parameter.module.omhdfshost-usage:

.. code-block:: none

   $ModLoad omhdfs
   $omhdfsHost hdfs01.example.net
   $omhdfsFileName /var/log/hdfs/system.log
   # write all messages to the specified HDFS host
   *.* :omhdfs:

Legacy names (for reference)
----------------------------
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfshost:

- ``$OMHDFSHost`` — maps to ``omhdfsHost``
  (status: legacy)

.. index::
   single: omhdfs; $OMHDFSHost
   single: $OMHDFSHost

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

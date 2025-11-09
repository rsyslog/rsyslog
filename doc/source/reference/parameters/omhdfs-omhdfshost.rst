.. _param-omhdfs-omhdfshost:
.. _omhdfs.parameter.module.omhdfshost:

OMHDFSHost
==========

.. index::
   single: omhdfs; OMHDFSHost
   single: OMHDFSHost

.. summary-start

Selects the hostname or IP address of the HDFS system omhdfs connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: OMHDFSHost
:Scope: module
:Type: word
:Default: module=default
:Required?: no
:Introduced: Not documented

Description
-----------
Name or IP address of the HDFS host to connect to.

Module usage
------------
.. _param-omhdfs-module-omhdfshost:
.. _omhdfs.parameter.module.omhdfshost-usage:

.. code-block:: rsyslog

   $ModLoad omhdfs
   $omhdfsHost hdfs01.example.net

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfshost:

- $OMHDFSHost — maps to OMHDFSHost (status: legacy)

.. index::
   single: omhdfs; $OMHDFSHost
   single: $OMHDFSHost

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

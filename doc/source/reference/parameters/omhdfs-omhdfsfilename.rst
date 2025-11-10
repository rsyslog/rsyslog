.. _param-omhdfs-omhdfsfilename:
.. _omhdfs.parameter.module.omhdfsfilename:

omhdfsFileName
==============

.. index::
   single: omhdfs; omhdfsFileName
   single: omhdfsFileName

.. summary-start

Sets the HDFS path of the file that omhdfs writes log messages to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: omhdfsFileName
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not documented

Description
-----------
The name of the file to which the output data shall be written.

Module usage
------------
.. _param-omhdfs-module-omhdfsfilename:
.. _omhdfs.parameter.module.omhdfsfilename-usage:

.. code-block:: rsyslog

   $ModLoad omhdfs
   $omhdfsFileName /var/log/hdfs/system.log

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfsfilename:

- $OMHDFSFileName — maps to omhdfsFileName (status: legacy)

.. index::
   single: omhdfs; $OMHDFSFileName
   single: $OMHDFSFileName

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

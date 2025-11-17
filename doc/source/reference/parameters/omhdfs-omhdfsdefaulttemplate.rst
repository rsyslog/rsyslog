.. _param-omhdfs-omhdfsdefaulttemplate:
.. _omhdfs.parameter.module.omhdfsdefaulttemplate:

omhdfsDefaultTemplate
=====================

.. index::
   single: omhdfs; omhdfsDefaultTemplate
   single: omhdfsDefaultTemplate

.. summary-start

Sets the template omhdfs applies when no template is specified for an action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: omhdfsDefaultTemplate
:Scope: module
:Type: word
:Default: RSYSLOG_FileFormat
:Required?: no
:Introduced: Not documented

Description
-----------
Default template to be used when none is specified. This saves the work of
specifying the same template repeatedly. Of course, the default
template can be overwritten by specifying a template directly on an action.

Module usage
------------
.. _omhdfs.parameter.module.omhdfsdefaulttemplate-usage:

.. code-block:: none

   $ModLoad omhdfs
   $omhdfsFileName /var/log/hdfs/system.log
   $omhdfsDefaultTemplate RSYSLOG_FileFormat
   # write all messages to HDFS using the custom default template
   *.* :omhdfs:

Legacy names (for reference)
----------------------------
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfsdefaulttemplate:

- ``$OMHDFSDefaultTemplate`` — maps to ``omhdfsDefaultTemplate``
  (status: legacy)

.. index::
   single: omhdfs; $OMHDFSDefaultTemplate
   single: $OMHDFSDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

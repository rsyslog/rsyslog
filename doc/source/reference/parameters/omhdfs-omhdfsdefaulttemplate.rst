.. _param-omhdfs-omhdfsdefaulttemplate:
.. _omhdfs.parameter.module.omhdfsdefaulttemplate:

OMHDFSDefaultTemplate
=====================

.. index::
   single: omhdfs; OMHDFSDefaultTemplate
   single: OMHDFSDefaultTemplate

.. summary-start

Sets the template omhdfs applies when no template is specified for an action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhdfs`.

:Name: OMHDFSDefaultTemplate
:Scope: module
:Type: word
:Default: module=RSYSLOG_FileFormat
:Required?: no
:Introduced: Not documented

Description
-----------
Default template to be used when none is specified. This saves the work of
specifying the same template ever and ever again. Of course, the default
template can be overwritten via the usual method.

Module usage
------------
.. _param-omhdfs-module-omhdfsdefaulttemplate:
.. _omhdfs.parameter.module.omhdfsdefaulttemplate-usage:

.. code-block:: rsyslog

   $ModLoad omhdfs
   $omhdfsDefaultTemplate RSYSLOG_FileFormat

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omhdfs.parameter.legacy.omhdfsdefaulttemplate:

- $OMHDFSDefaultTemplate — maps to OMHDFSDefaultTemplate (status: legacy)

.. index::
   single: omhdfs; $OMHDFSDefaultTemplate
   single: $OMHDFSDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omhdfs`.

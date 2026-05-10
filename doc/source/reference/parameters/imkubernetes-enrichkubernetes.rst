.. _param-imkubernetes-enrichkubernetes:
.. _imkubernetes.parameter.module.enrichkubernetes:

EnrichKubernetes
================

.. index::
   single: imkubernetes; EnrichKubernetes
   single: EnrichKubernetes

.. summary-start

Enable Kubernetes API lookups for pod metadata enrichment; default ``on``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: EnrichKubernetes
:Scope: module
:Type: binary
:Default: ``on``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When enabled, ``imkubernetes`` fetches pod metadata from the Kubernetes API and
adds it under ``$!kubernetes``. Turn this off to operate purely from the file
path and log record contents.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" EnrichKubernetes="off")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

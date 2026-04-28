.. _param-imkubernetes-cacheentryttl:
.. _imkubernetes.parameter.module.cacheentryttl:

CacheEntryTtl
=============

.. index::
   single: imkubernetes; CacheEntryTtl
   single: CacheEntryTtl

.. summary-start

Seconds to keep pod metadata in the in-memory cache; default ``300``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: CacheEntryTtl
:Scope: module
:Type: integer
:Default: ``300``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When Kubernetes enrichment is enabled, successful pod lookups are cached for
this many seconds before the entry expires and must be re-fetched.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" CacheEntryTtl="300")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.

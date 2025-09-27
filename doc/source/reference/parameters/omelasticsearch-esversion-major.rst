.. _param-omelasticsearch-esversion-major:
.. _omelasticsearch.parameter.module.esversion-major:
.. _omelasticsearch.parameter.module.esVersion.major:

esVersion.major
===============

.. index::
   single: omelasticsearch; esVersion.major
   single: esVersion.major

.. summary-start

Deprecated manual override for the detected Elasticsearch/OpenSearch major
version.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: esVersion.major
:Scope: action
:Type: integer
:Default: action=0 (overridden when detection succeeds)
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
.. warning::

   This parameter is deprecated as of rsyslog 8.2510.0.  The module performs a
   best-effort probe at
   startup to discover the target platform (Elasticsearch or OpenSearch) and
   its version, and ``esVersion.major`` is automatically updated with the
   detected major version when the probe succeeds.

.. versionchanged:: 8.2510.0
   Automatically overridden when startup platform detection succeeds.

This setting is only consulted when detection fails or when no servers are
reachable during startup.  Administrators may keep it configured as a fallback
for air-gapped or permission-restricted environments, but future releases may
remove the option entirely.

Action usage
------------
.. _param-omelasticsearch-action-esversion-major:
.. _omelasticsearch.parameter.action.esversion-major:
.. code-block:: rsyslog

   action(type="omelasticsearch" esVersion.major="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

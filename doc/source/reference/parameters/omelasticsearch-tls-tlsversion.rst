.. _param-omelasticsearch-tls-tlsversion:
.. _omelasticsearch.parameter.module.tls-tlsversion:
.. _omelasticsearch.parameter.module.tls.tlsversion:

tls.tlsversion
==============

.. index::
   single: omelasticsearch; tls.tlsversion
   single: tls.tlsversion

.. summary-start

Minimum TLS protocol version for the Elasticsearch connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.tlsversion
:Scope: action
:Type: word
:Default: none (libcurl default)
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Sets the minimum (floor) TLS version accepted for the connection to
Elasticsearch. Accepted values are ``TLSv1.2`` and ``TLSv1.3``.

An unknown value is rejected at configuration load time and prevents rsyslog
from starting.

When omitted, libcurl negotiates the highest version supported by both sides,
typically TLS 1.3 on modern systems.

.. note::

   Setting this to ``TLSv1.2`` downgrades the effective floor and should be
   avoided unless the server does not support TLS 1.3.

Action usage
------------
.. _param-omelasticsearch-action-tls-tlsversion:
.. _omelasticsearch.parameter.action.tls-tlsversion:
.. code-block:: rsyslog

   action(type="omelasticsearch" tls.tlsversion="TLSv1.3")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`,
:ref:`param-omelasticsearch-tls-ciphersuites`,
:ref:`param-omelasticsearch-tls-keyexchangegroups`.

.. _param-omelasticsearch-tls-ciphersuites:
.. _omelasticsearch.parameter.module.tls-ciphersuites:
.. _omelasticsearch.parameter.module.tls.ciphersuites:

tls.ciphersuites
================

.. index::
   single: omelasticsearch; tls.ciphersuites
   single: tls.ciphersuites

.. summary-start

TLS 1.3 cipher suite selection for the Elasticsearch connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.ciphersuites
:Scope: action
:Type: word
:Default: none (libcurl/OpenSSL default)
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Passes a colon-separated list of TLS 1.3 cipher suite names to libcurl
(``CURLOPT_TLS13_CIPHERS``). The string is validated by OpenSSL at connection
time; an invalid name causes the handshake to fail.

Requires libcurl 7.61.0 or later. If rsyslog was built against an older
libcurl, configuring this parameter has no effect and a warning is logged at
startup.

Action usage
------------
.. _param-omelasticsearch-action-tls-ciphersuites:
.. _omelasticsearch.parameter.action.tls-ciphersuites:
.. code-block:: rsyslog

   action(type="omelasticsearch" tls.ciphersuites="TLS_AES_256_GCM_SHA384")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`,
:ref:`param-omelasticsearch-tls-tlsversion`,
:ref:`param-omelasticsearch-tls-keyexchangegroups`.

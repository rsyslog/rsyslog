.. _param-omelasticsearch-tls-keyexchangegroups:
.. _omelasticsearch.parameter.module.tls-keyexchangegroups:
.. _omelasticsearch.parameter.module.tls.keyexchangegroups:

tls.keyexchangegroups
=====================

.. index::
   single: omelasticsearch; tls.keyexchangegroups
   single: tls.keyexchangegroups

.. summary-start

Key exchange group preference list, enabling post-quantum hybrid KEMs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.keyexchangegroups
:Scope: action
:Type: word
:Default: none (libcurl/OpenSSL default)
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Passes a colon-separated list of named key-exchange groups to libcurl
(``CURLOPT_SSL_EC_CURVES``). Groups are advertised in order of preference
in the TLS ClientHello.

This is the primary knob for post-quantum cryptography (PQC) readiness.
Setting it to ``X25519MLKEM768:X25519`` requests the hybrid ML-KEM / X25519
group first, falling back to classical X25519 if the server does not support
it. For a hard PQC-only policy, omit the ``:X25519`` suffix — connections to
non-PQC servers will then fail.

PQC key exchange requires:

* libcurl 7.73.0 or later (``CURLOPT_SSL_EC_CURVES`` support)
* OpenSSL 3.x with the `OQS provider <https://github.com/open-quantum-safe/oqs-provider>`_ installed

If rsyslog was built against libcurl older than 7.73, configuring this
parameter has no effect and a warning is logged at configuration load.

Action usage
------------
.. _param-omelasticsearch-action-tls-keyexchangegroups:
.. _omelasticsearch.parameter.action.tls-keyexchangegroups:
.. code-block:: rsyslog

   action(type="omelasticsearch"
          usehttps="on"
          tls.cacert="/etc/pki/tls/certs/ca-bundle.crt"
          tls.tlsversion="TLSv1.3"
          tls.keyexchangegroups="X25519MLKEM768:X25519")

YAML usage
----------
.. code-block:: yaml

   actions:
     - type: omelasticsearch
       usehttps: "on"
       tls.cacert: "/etc/pki/tls/certs/ca-bundle.crt"
       tls.tlsversion: "TLSv1.3"
       tls.keyexchangegroups: "X25519MLKEM768:X25519"

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`,
:ref:`param-omelasticsearch-tls-tlsversion`,
:ref:`param-omelasticsearch-tls-ciphersuites`.

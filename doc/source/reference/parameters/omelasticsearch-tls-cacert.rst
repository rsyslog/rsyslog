.. _param-omelasticsearch-tls-cacert:
.. _omelasticsearch.parameter.module.tls-cacert:
.. _omelasticsearch.parameter.module.tls.cacert:

tls.cacert
==========

.. index::
   single: omelasticsearch; tls.cacert
   single: tls.cacert

.. summary-start

CA certificate file used to verify the Elasticsearch server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.cacert
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Path to a PEM file containing the CA certificate that issued the server certificate.

Action usage
------------
.. _param-omelasticsearch-action-tls-cacert:
.. _omelasticsearch.parameter.action.tls-cacert:
.. code-block:: rsyslog

   action(type="omelasticsearch" tls.cacert="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

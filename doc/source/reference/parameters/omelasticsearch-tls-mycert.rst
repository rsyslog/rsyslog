.. _param-omelasticsearch-tls-mycert:
.. _omelasticsearch.parameter.module.tls-mycert:
.. _omelasticsearch.parameter.module.tls.mycert:

tls.mycert
==========

.. index::
   single: omelasticsearch; tls.mycert
   single: tls.mycert

.. summary-start

Client certificate for mutual TLS authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.mycert
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
PEM file containing the client certificate presented to Elasticsearch.

Action usage
------------
.. _param-omelasticsearch-action-tls-mycert:
.. _omelasticsearch.parameter.action.tls-mycert:
.. code-block:: rsyslog

   action(type="omelasticsearch" tls.mycert="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

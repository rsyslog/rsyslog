.. _param-omelasticsearch-apikey:
.. _omelasticsearch.parameter.module.apikey:

apikey
======

.. index::
   single: omelasticsearch; apikey
   single: apikey

.. summary-start

Supplies the value for the ``Authorization: ApiKey`` HTTP header.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: apikey
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: 8.2406.0

Description
-----------
Accepts the API key material expected by Elasticsearch's ``ApiKey`` authentication scheme.
The configured value is appended to the ``Authorization: ApiKey`` header without modification,
so provide the base64-encoded ``<id>:<api_key>`` string exactly as documented by Elasticsearch.

The API key setting cannot be combined with :ref:`param-omelasticsearch-uid` or
:ref:`param-omelasticsearch-pwd`.

Action usage
------------
.. _param-omelasticsearch-action-apikey:
.. _omelasticsearch.parameter.action.apikey:
.. code-block:: rsyslog

   action(type="omelasticsearch" apikey="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

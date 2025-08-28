.. _param-omelasticsearch-allowunsignedcerts:
.. _omelasticsearch.parameter.module.allowunsignedcerts:

allowunsignedcerts
==================

.. index::
   single: omelasticsearch; allowunsignedcerts
   single: allowunsignedcerts

.. summary-start

Disable TLS peer verification (insecure, for testing only).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: allowunsignedcerts
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Sets the curl `CURLOPT_SSL_VERIFYPEER` option to `0`. Strongly discouraged outside of testing.

Action usage
------------
.. _param-omelasticsearch-action-allowunsignedcerts:
.. _omelasticsearch.parameter.action.allowunsignedcerts:
.. code-block:: rsyslog

   action(type="omelasticsearch" allowunsignedcerts="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

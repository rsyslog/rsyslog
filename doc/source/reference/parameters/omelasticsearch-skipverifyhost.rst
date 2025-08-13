.. _param-omelasticsearch-skipverifyhost:
.. _omelasticsearch.parameter.module.skipverifyhost:

skipverifyhost
==============

.. index::
   single: omelasticsearch; skipverifyhost
   single: skipverifyhost

.. summary-start

Disable TLS host name verification (insecure, for testing).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: skipverifyhost
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Sets the curl `CURLOPT_SSL_VERIFYHOST` option to `0`. Use only for debugging.

Action usage
------------
.. _param-omelasticsearch-action-skipverifyhost:
.. _omelasticsearch.parameter.action.skipverifyhost:
.. code-block:: rsyslog

   action(type="omelasticsearch" skipverifyhost="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

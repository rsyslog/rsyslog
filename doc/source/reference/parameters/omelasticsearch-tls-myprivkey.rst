.. _param-omelasticsearch-tls-myprivkey:
.. _omelasticsearch.parameter.module.tls-myprivkey:
.. _omelasticsearch.parameter.module.tls.myprivkey:

tls.myprivkey
=============

.. index::
   single: omelasticsearch; tls.myprivkey
   single: tls.myprivkey

.. summary-start

Unencrypted private key for `tls.mycert`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: tls.myprivkey
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
PEM file containing the private key corresponding to the client certificate. The key must not be encrypted.

Action usage
------------
.. _param-omelasticsearch-action-tls-myprivkey:
.. _omelasticsearch.parameter.action.tls-myprivkey:
.. code-block:: rsyslog

   action(type="omelasticsearch" tls.myprivkey="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.

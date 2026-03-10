.. _param-omazuredce-client_secret:
.. _omazuredce.parameter.action.client_secret:

.. meta::
   :description: Reference for the omazuredce client_secret parameter.
   :keywords: rsyslog, omazuredce, client_secret, azure, entra

client_secret
=============

.. index::
   single: omazuredce; client_secret
   single: client_secret

.. summary-start

Supplies the client secret paired with ``client_id`` for OAuth token requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: client_secret
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``client_secret`` provides the shared secret used by Microsoft Entra client
credentials authentication. Treat this value as sensitive and avoid storing
real production secrets in example configurations or version control.

Action usage
------------
.. _omazuredce.parameter.action.client_secret-usage:

.. code-block:: rsyslog

   action(type="omazuredce" client_secret="<client-secret>" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.

.. _param-omazuredce-client_id:
.. _omazuredce.parameter.action.client_id:

.. meta::
   :description: Reference for the omazuredce client_id parameter.
   :keywords: rsyslog, omazuredce, client_id, azure, entra

client_id
=========

.. index::
   single: omazuredce; client_id
   single: client_id

.. summary-start

Specifies the Microsoft Entra application client ID used for OAuth token
requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: client_id
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``client_id`` identifies the Azure application that requests access tokens for
the Logs Ingestion API. It is used together with ``client_secret`` and
``tenant_id``.

Action usage
------------
.. _omazuredce.parameter.action.client_id-usage:

.. code-block:: rsyslog

   action(type="omazuredce" client_id="<application-id>" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
